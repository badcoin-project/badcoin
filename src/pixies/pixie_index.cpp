// Copyright (c) 2026 The BadCoin contributors

#include <pixies/pixie_index.h>
#include <pixies/pixie_db.h>

#include <algorithm>
#include <base58.h>
#include <chain.h>
#include <chainparams.h>
#include <util.h>
#include <validation.h>

namespace pixies {

CPixieIndex& CPixieIndex::Instance()
{
    static CPixieIndex instance;
    return instance;
}

CPixieIndex::CPixieIndex() : activation_height(0)
{
    global.total_minted = 0;
    global.next_pixie_id = 1;
}

CPixieIndex::~CPixieIndex() = default;

void CPixieIndex::Clear()
{
    LOCK(cs);
    global = {0, 1};
    by_id.clear();
    control_outpoint_to_id.clear();
    by_address.clear();
    undo_by_height.clear();
}

void CPixieIndex::OpenDB(bool fWipe)
{
    if (db && !fWipe)
        return;
    db = MakeUnique<CPixieDB>(PIXIE_DB_CACHE, false, fWipe);
}

bool CPixieIndex::LoadFromDB(const uint256& expected_tip)
{
    if (!db || db->IsEmpty())
        return false;

    GlobalPixieStats loaded_global;
    int loaded_activation = 0;
    std::map<uint32_t, PixieRecord> loaded_by_id;
    std::map<COutPoint, uint32_t> loaded_control;
    std::map<std::string, AddressPixieStats> loaded_addr;
    uint256 best_block;

    if (!db->Load(loaded_global, loaded_activation, loaded_by_id, loaded_control, loaded_addr, best_block))
        return false;
    if (best_block != expected_tip)
        return false;

    LOCK(cs);
    global = loaded_global;
    activation_height = loaded_activation;
    by_id = std::move(loaded_by_id);
    control_outpoint_to_id = std::move(loaded_control);
    by_address = std::move(loaded_addr);
    undo_by_height.clear();
    return true;
}

void CPixieIndex::FlushToDisk(const uint256& block_hash)
{
    if (!db)
        return;

    LOCK(cs);
    db->Save(global, activation_height, by_id, control_outpoint_to_id, by_address, block_hash, true);
}

bool CPixieIndex::GetPersistedTip(uint256& out) const
{
    if (!db)
        return false;
    return db->ReadBestBlock(out);
}

void CPixieIndex::ReindexToHeight(const CChainParams& params, int target_height)
{
    Clear();
    if (target_height < activation_height)
        return;

    CBlock block;
    PixieRejectCode reject;
    LOCK(cs_main);
    for (int h = std::max(activation_height, 0); h <= target_height; ++h) {
        CBlockIndex* pindex = chainActive[h];
        if (!ReadBlockFromDisk(block, pindex, params.GetConsensus())) {
            throw std::runtime_error("ReindexToHeight: ReadBlockFromDisk failed");
        }
        if (!ConnectBlock(block.vtx, h, reject)) {
            throw std::runtime_error(std::string("ReindexToHeight: ConnectBlock failed: ") +
                                     PixieRejectCodeString(reject));
        }
    }
    if (chainActive[target_height]) {
        FlushToDisk(chainActive[target_height]->GetBlockHash());
    }
}

void CPixieIndex::ReindexFromChain(const CChainParams& params)
{
    int tip_height = 0;
    {
        LOCK(cs_main);
        if (chainActive.Tip())
            tip_height = chainActive.Tip()->nHeight;
    }
    LogPrintf("Rebuilding pixie index from chain (height %d)...\n", tip_height);
    ReindexToHeight(params, tip_height);
    LogPrintf("Pixie index rebuild complete (%u pixies).\n", GetGlobalStats().total_minted);
}

void InitPixieIndex(const CChainParams& params, bool fReindexPixies)
{
    CPixieIndex& idx = CPixieIndex::Instance();
    idx.OpenDB(fReindexPixies);
    if (fReindexPixies) {
        idx.Clear();
        return;
    }

    uint256 tip_hash;
    int tip_height = -1;
    {
        LOCK(cs_main);
        if (chainActive.Tip()) {
            tip_hash = chainActive.Tip()->GetBlockHash();
            tip_height = chainActive.Tip()->nHeight;
        }
    }

    if (tip_height < 0)
        return;

    if (!idx.LoadFromDB(tip_hash)) {
        LogPrintf("Pixie index: no matching DB at chain tip, will rebuild after block import\n");
        idx.Clear();
    }
}

void FinalizePixieIndexAfterBlockImport(const CChainParams& params, bool fReindexPixies)
{
    CPixieIndex& idx = CPixieIndex::Instance();
    uint256 tip_hash;
    {
        LOCK(cs_main);
        if (!chainActive.Tip())
            return;
        tip_hash = chainActive.Tip()->GetBlockHash();
    }

    if (fReindexPixies) {
        idx.ReindexFromChain(params);
        return;
    }

    uint256 db_tip;
    if (!idx.GetPersistedTip(db_tip) || db_tip != tip_hash) {
        idx.ReindexFromChain(params);
    }
}

bool CPixieIndex::IsActive(int height) const
{
    return height >= activation_height;
}

static std::string AddressFromScript(const CScript& script)
{
    CTxDestination dest;
    if (!ExtractDestination(script, dest))
        return "";
    return EncodeDestination(dest);
}

static COutPoint MakeOutPoint(const uint256& txid, uint32_t vout)
{
    return COutPoint(txid, vout);
}

bool CPixieIndex::ConnectBlock(const std::vector<CTransactionRef>& vtx, int height,
                               PixieRejectCode& reject)
{
    reject = PixieRejectCode::OK;
    if (!IsActive(height))
        return true;

    LOCK(cs);
    const GlobalPixieStats snap_global = global;
    const std::map<uint32_t, PixieRecord> snap_by_id = by_id;
    const std::map<COutPoint, uint32_t> snap_control = control_outpoint_to_id;
    const std::map<std::string, AddressPixieStats> snap_addr = by_address;

    BlockUndo undo;
    undo.prev_global = snap_global;
    undo.prev_address_stats = snap_addr;

    for (size_t i = 0; i < vtx.size(); ++i) {
        if (vtx[i]->IsCoinBase())
            continue;
        if (!ConnectTransaction(*vtx[i], height, vtx[i]->GetHash(), undo, reject)) {
            global = snap_global;
            by_id = snap_by_id;
            control_outpoint_to_id = snap_control;
            by_address = snap_addr;
            return false;
        }
    }

    undo_by_height[height] = undo;
    return true;
}

bool CPixieIndex::ConnectTransaction(const CTransaction& tx, int height, const uint256& txid,
                                     BlockUndo& undo, PixieRejectCode& reject)
{
    reject = PixieRejectCode::OK;

    int mint_vout = -1;
    int transfer_vout = -1;
    std::vector<uint8_t> mint_payload;
    std::vector<uint8_t> transfer_payload;

    for (size_t i = 0; i < tx.vout.size(); ++i) {
        std::vector<uint8_t> payload;
        if (!ExtractOpReturnPayload(tx.vout[i].scriptPubKey, payload))
            continue;
        if (payload.size() >= 5 && memcmp(payload.data(), PIXIE_PROTOCOL_TAG, 4) == 0) {
            if (payload[4] == OP_MINT && mint_vout < 0) {
                mint_vout = static_cast<int>(i);
                mint_payload = payload;
            } else if (payload[4] == OP_TRANSFER && transfer_vout < 0) {
                transfer_vout = static_cast<int>(i);
                transfer_payload = payload;
            }
        }
    }

    if (mint_vout < 0 && transfer_vout < 0)
        return true;

    if (mint_vout >= 0) {
        if (transfer_vout >= 0) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        PixieMintData mint;
        if (!ParseMintPayload(mint_payload, mint)) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        if (mint.world_id > 7 || mint.palette_id > 4 || mint.base_type_id > 3) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        if ((mint.traits & 0x00000300) != 0 || (mint.traits & 0xFFC00000) != 0) {
            reject = PixieRejectCode::TRAITS;
            return false;
        }
        if (!ValidateMintName(mint.name, reject))
            return false;

        int control_vout = FindControlUtxoVout(tx);
        if (control_vout < 0) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }

        return ApplyMint(tx, height, txid, mint, mint_vout, control_vout, undo, reject);
    }

    PixieTransferData xfer;
    if (!ParseTransferPayload(transfer_payload, xfer)) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }
    int control_vout = FindControlUtxoVout(tx);
    if (control_vout < 0) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }
    return ApplyTransfer(tx, height, txid, xfer, control_vout, undo, reject);
}

bool CPixieIndex::ApplyMint(const CTransaction& tx, int height, const uint256& txid,
                            const PixieMintData& mint, int op_return_vout, int control_vout,
                            BlockUndo& undo, PixieRejectCode& reject)
{
    if (!ValidateMint(tx, height, mint, control_vout, reject))
        return false;

    const CScript& control_script = tx.vout[control_vout].scriptPubKey;
    std::string minter = AddressFromScript(control_script);

    const uint32_t new_id = global.next_pixie_id;

    PixieRecord rec;
    rec.pixie_id = new_id;
    rec.world_id = mint.world_id;
    rec.palette_id = mint.palette_id;
    rec.base_type_id = mint.base_type_id;
    rec.traits = mint.traits;
    rec.pixel_data = mint.pixel_data;
    rec.name = mint.name;
    rec.control_txid = txid;
    rec.control_vout = static_cast<uint32_t>(control_vout);
    rec.current_owner = minter;
    rec.height = height;
    rec.mint_txid = txid;

    AddressPixieStats& stats = by_address[minter];
    by_id[new_id] = rec;
    control_outpoint_to_id[MakeOutPoint(txid, control_vout)] = new_id;
    stats.last_mint_height = height;
    stats.active_count += 1;
    global.total_minted += 1;
    global.next_pixie_id += 1;

    undo.minted_ids.push_back(new_id);
    return true;
}

bool CPixieIndex::ValidateMint(const CTransaction& tx, int height, const PixieMintData& mint,
                               int control_vout, PixieRejectCode& reject) const
{
    reject = PixieRejectCode::OK;
    if (global.total_minted >= PIXIE_GLOBAL_CAP) {
        reject = PixieRejectCode::SUPPLY_CAP;
        return false;
    }

    const CScript& control_script = tx.vout[control_vout].scriptPubKey;
    const std::string minter = AddressFromScript(control_script);
    if (minter.empty()) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }

    auto addr_it = by_address.find(minter);
    if (addr_it != by_address.end()) {
        const AddressPixieStats& stats = addr_it->second;
        if (stats.last_mint_height > 0 &&
            height - stats.last_mint_height < static_cast<int>(PIXIE_MINT_COOLDOWN_BLOCKS)) {
            reject = PixieRejectCode::COOLDOWN;
            return false;
        }
        if (stats.active_count >= PIXIE_MAX_ACTIVE_PER_ADDRESS) {
            reject = PixieRejectCode::ACTIVE_MINT_CAP;
            return false;
        }
    }

    const uint32_t new_id = global.next_pixie_id;
    const CAmount burn_expected = ExpectedMintBurnSats(new_id);
    if (!FindMintBurnOutput(tx, burn_expected, false)) {
        reject = PixieRejectCode::BURN;
        return false;
    }
    return true;
}

bool CPixieIndex::ValidateTransfer(const CTransaction& tx, const PixieTransferData& xfer,
                                   int new_control_vout, PixieRejectCode& reject) const
{
    reject = PixieRejectCode::OK;
    const uint32_t pixie_id = xfer.pixie_id;
    auto it = by_id.find(pixie_id);
    if (it == by_id.end()) {
        reject = PixieRejectCode::TRANSFER_NOT_FOUND;
        return false;
    }

    const PixieRecord& rec = it->second;
    const COutPoint spent = MakeOutPoint(rec.control_txid, rec.control_vout);
    bool found_input = false;
    for (const CTxIn& in : tx.vin) {
        if (in.prevout == spent) {
            found_input = true;
            break;
        }
    }
    if (!found_input) {
        reject = PixieRejectCode::TRANSFER_NOT_FOUND;
        return false;
    }

    const CScript& new_script = tx.vout[new_control_vout].scriptPubKey;
    const std::string new_owner = AddressFromScript(new_script);
    if (new_owner.empty()) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }

    auto new_it = by_address.find(new_owner);
    if (new_it != by_address.end() && new_it->second.active_count >= PIXIE_MAX_ACTIVE_PER_ADDRESS) {
        reject = PixieRejectCode::TRANSFER_ACTIVE_CAP;
        return false;
    }
    return true;
}

bool CPixieIndex::CheckPixieTx(const CTransaction& tx, int height, PixieRejectCode& reject) const
{
    LOCK(cs);
    reject = PixieRejectCode::OK;
    if (!IsActive(height))
        return true;

    int mint_vout = -1;
    int transfer_vout = -1;
    std::vector<uint8_t> mint_payload;
    std::vector<uint8_t> transfer_payload;

    for (size_t i = 0; i < tx.vout.size(); ++i) {
        std::vector<uint8_t> payload;
        if (!ExtractOpReturnPayload(tx.vout[i].scriptPubKey, payload))
            continue;
        if (payload.size() >= 5 && memcmp(payload.data(), PIXIE_PROTOCOL_TAG, 4) == 0) {
            if (payload[4] == OP_MINT && mint_vout < 0) {
                mint_vout = static_cast<int>(i);
                mint_payload = payload;
            } else if (payload[4] == OP_TRANSFER && transfer_vout < 0) {
                transfer_vout = static_cast<int>(i);
                transfer_payload = payload;
            }
        }
    }

    if (mint_vout < 0 && transfer_vout < 0)
        return true;

    if (mint_vout >= 0) {
        if (transfer_vout >= 0) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        PixieMintData mint;
        if (!ParseMintPayload(mint_payload, mint)) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        if (mint.world_id > 7 || mint.palette_id > 4 || mint.base_type_id > 3) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        if (!ValidateMintTraits(mint.traits)) {
            reject = PixieRejectCode::TRAITS;
            return false;
        }
        if (!ValidateMintName(mint.name, reject))
            return false;
        int control_vout = FindControlUtxoVout(tx);
        if (control_vout < 0) {
            reject = PixieRejectCode::MALFORMED;
            return false;
        }
        return ValidateMint(tx, height, mint, control_vout, reject);
    }

    PixieTransferData xfer;
    if (!ParseTransferPayload(transfer_payload, xfer)) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }
    int control_vout = FindControlUtxoVout(tx);
    if (control_vout < 0) {
        reject = PixieRejectCode::MALFORMED;
        return false;
    }
    return ValidateTransfer(tx, xfer, control_vout, reject);
}

bool CPixieIndex::ApplyTransfer(const CTransaction& tx, int height, const uint256& txid,
                                const PixieTransferData& xfer, int new_control_vout,
                                BlockUndo& undo, PixieRejectCode& reject)
{
    if (!ValidateTransfer(tx, xfer, new_control_vout, reject))
        return false;

    const uint32_t pixie_id = xfer.pixie_id;
    PixieRecord& rec = by_id.find(pixie_id)->second;
    const COutPoint spent = MakeOutPoint(rec.control_txid, rec.control_vout);
    const CScript& new_script = tx.vout[new_control_vout].scriptPubKey;
    const std::string new_owner = AddressFromScript(new_script);

    undo.restored_pixies.push_back(std::make_pair(pixie_id, rec));

    std::string old_owner = rec.current_owner;
    if (!old_owner.empty()) {
        AddressPixieStats& old_stats = by_address[old_owner];
        if (old_stats.active_count > 0)
            old_stats.active_count -= 1;
    }

    control_outpoint_to_id.erase(spent);
    rec.control_txid = txid;
    rec.control_vout = static_cast<uint32_t>(new_control_vout);
    rec.current_owner = new_owner;
    control_outpoint_to_id[MakeOutPoint(txid, new_control_vout)] = pixie_id;
    AddressPixieStats& new_stats = by_address[new_owner];
    new_stats.active_count += 1;

    return true;
}

void CPixieIndex::DisconnectBlock(int height, const CChainParams* params)
{
    BlockUndo undo_copy;
    bool have_undo = false;
    {
        LOCK(cs);
        auto it = undo_by_height.find(height);
        if (it == undo_by_height.end()) {
            if (params == nullptr) {
                LogPrintf("Pixie index: missing undo at height %d with no chain params\n", height);
                return;
            }
        } else {
            undo_copy = it->second;
            have_undo = true;
        }
    }

    if (!have_undo) {
        // Do not call ReindexToHeight here. VerifyDB disconnects tip blocks
        // without pixie undos present; a full reindex rehashes the chain and
        // freezes startup at "Verifying blocks… 1%". Skip and let a later
        // ConnectBlock / Load path rebuild the index if needed.
        LogPrintf("Pixie index: missing undo at height %d, skipping disconnect (no reindex)\n", height);
        return;
    }

    LOCK(cs);
    const BlockUndo& undo = undo_copy;
    global = undo.prev_global;
    by_address = undo.prev_address_stats;

    for (const auto& p : undo.restored_pixies) {
        const uint32_t id = p.first;
        const PixieRecord& old = p.second;
        auto cur = by_id.find(id);
        if (cur != by_id.end()) {
            control_outpoint_to_id.erase(MakeOutPoint(cur->second.control_txid, cur->second.control_vout));
            by_id[id] = old;
            control_outpoint_to_id[MakeOutPoint(old.control_txid, old.control_vout)] = id;
        }
    }

    for (uint32_t id : undo.minted_ids) {
        auto cur = by_id.find(id);
        if (cur != by_id.end()) {
            control_outpoint_to_id.erase(MakeOutPoint(cur->second.control_txid, cur->second.control_vout));
            by_id.erase(id);
        }
    }

    undo_by_height.erase(height);
}

bool CPixieIndex::GetPixie(uint32_t pixie_id, PixieRecord& out) const
{
    LOCK(cs);
    auto it = by_id.find(pixie_id);
    if (it == by_id.end())
        return false;
    out = it->second;
    return true;
}

GlobalPixieStats CPixieIndex::GetGlobalStats() const
{
    LOCK(cs);
    return global;
}

void CPixieIndex::SetTestGlobalStats(uint32_t total_minted, uint32_t next_pixie_id)
{
    LOCK(cs);
    global.total_minted = total_minted;
    global.next_pixie_id = next_pixie_id;
}

uint32_t CPixieIndex::GetActiveCount(const std::string& address) const
{
    LOCK(cs);
    auto it = by_address.find(address);
    if (it == by_address.end())
        return 0;
    return it->second.active_count;
}

std::vector<PixieRecord> CPixieIndex::ListByAddress(const std::string& address) const
{
    LOCK(cs);
    std::vector<PixieRecord> result;
    for (const auto& kv : by_id) {
        if (kv.second.current_owner == address)
            result.push_back(kv.second);
    }
    return result;
}

int CPixieIndex::GetMintCooldownBlocksRemaining(const std::string& address, int current_height) const
{
    LOCK(cs);
    auto it = by_address.find(address);
    if (it == by_address.end() || it->second.last_mint_height <= 0)
        return -1;
    const int elapsed = current_height - it->second.last_mint_height;
    if (elapsed >= static_cast<int>(PIXIE_MINT_COOLDOWN_BLOCKS))
        return -1;
    return static_cast<int>(PIXIE_MINT_COOLDOWN_BLOCKS) - elapsed;
}

std::vector<PixieRecord> CPixieIndex::ListAll() const
{
    LOCK(cs);
    std::vector<PixieRecord> result;
    result.reserve(by_id.size());
    for (const auto& kv : by_id)
        result.push_back(kv.second);
    std::sort(result.begin(), result.end(),
              [](const PixieRecord& a, const PixieRecord& b) { return a.pixie_id < b.pixie_id; });
    return result;
}

} // namespace pixies

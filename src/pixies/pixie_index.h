// Copyright (c) 2026 The BadCoin contributors

#ifndef BITCOIN_PIXIES_PIXIE_INDEX_H
#define BITCOIN_PIXIES_PIXIE_INDEX_H

#include <pixies/pixies.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <uint256.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class CChainParams;

namespace pixies {

class CPixieDB;

struct PixieRecord {
    uint32_t pixie_id;
    uint8_t world_id;
    uint8_t palette_id;
    uint8_t base_type_id;
    uint32_t traits;
    std::vector<uint8_t> pixel_data;
    std::string name;
    uint256 control_txid;
    uint32_t control_vout;
    std::string current_owner;
    int height;
    uint256 mint_txid;
};

struct AddressPixieStats {
    int last_mint_height;
    uint32_t active_count;
};

struct GlobalPixieStats {
    uint32_t total_minted;
    uint32_t next_pixie_id;
};

class CPixieIndex
{
public:
    static CPixieIndex& Instance();

    void Clear();

    bool IsActive(int height) const;
    void SetActivationHeight(int height) { activation_height = height; }
    int ActivationHeight() const { return activation_height; }

    /** Process all txs in a connected block (skips coinbase). */
    bool ConnectBlock(const std::vector<CTransactionRef>& vtx, int height,
                      PixieRejectCode& reject);

    /** Validate pixie semantics at block height without mutating index. Returns true if non-pixie or valid. */
    bool CheckPixieTx(const CTransaction& tx, int height, PixieRejectCode& reject) const;

    /** Regtest harness only: set global counters without creating pixie records. */
    void SetTestGlobalStats(uint32_t total_minted, uint32_t next_pixie_id);

    void OpenDB(bool fWipe = false);
    bool LoadFromDB(const uint256& expected_tip);
    void FlushToDisk(const uint256& block_hash);
    bool GetPersistedTip(uint256& out) const;
    void ReindexFromChain(const CChainParams& params);
    void ReindexToHeight(const CChainParams& params, int target_height);
    void DisconnectBlock(int height, const CChainParams* params = nullptr);

    bool GetPixie(uint32_t pixie_id, PixieRecord& out) const;
    GlobalPixieStats GetGlobalStats() const;
    uint32_t GetActiveCount(const std::string& address) const;
    /** -1 if address may mint now; else blocks until cooldown ends. */
    int GetMintCooldownBlocksRemaining(const std::string& address, int current_height) const;
    std::vector<PixieRecord> ListByAddress(const std::string& address) const;
    std::vector<PixieRecord> ListAll() const;

private:
    CPixieIndex();
    ~CPixieIndex();

    mutable CCriticalSection cs;
    int activation_height;

    GlobalPixieStats global;
    std::map<uint32_t, PixieRecord> by_id;
    std::map<COutPoint, uint32_t> control_outpoint_to_id;
    std::map<std::string, AddressPixieStats> by_address;

    struct BlockUndo {
        std::vector<uint32_t> minted_ids;
        std::vector<std::pair<uint32_t, PixieRecord>> restored_pixies;
        std::map<std::string, AddressPixieStats> prev_address_stats;
        GlobalPixieStats prev_global;
    };
    std::map<int, BlockUndo> undo_by_height;

    bool ConnectTransaction(const CTransaction& tx, int height, const uint256& txid,
                            BlockUndo& undo, PixieRejectCode& reject);

    bool ApplyMint(const CTransaction& tx, int height, const uint256& txid,
                   const PixieMintData& mint, int op_return_vout, int control_vout,
                   BlockUndo& undo, PixieRejectCode& reject);
    bool ApplyTransfer(const CTransaction& tx, int height, const uint256& txid,
                       const PixieTransferData& xfer, int control_vout,
                       BlockUndo& undo, PixieRejectCode& reject);

    bool ValidateMint(const CTransaction& tx, int height, const PixieMintData& mint,
                      int control_vout, PixieRejectCode& reject) const;
    bool ValidateTransfer(const CTransaction& tx, const PixieTransferData& xfer,
                          int new_control_vout, PixieRejectCode& reject) const;

    std::unique_ptr<CPixieDB> db;
    static constexpr size_t PIXIE_DB_CACHE = 2 << 20;
};

/** Open pixie DB and load from disk when chain tip is available. */
void InitPixieIndex(const CChainParams& params, bool fReindexPixies);

/** After block import / ActivateBestChain: rebuild index only if -reindex-pixies. */
void FinalizePixieIndexAfterBlockImport(const CChainParams& params, bool fReindexPixies);

} // namespace pixies

#endif

// Copyright (c) 2026 The BadCoin contributors

#include <pixies/pixie_db.h>

#include <util.h>

namespace pixies {

namespace {

static const char DB_PIXIE_GLOBAL = 'G';
static const char DB_PIXIE_RECORD = 'P';
static const char DB_PIXIE_CONTROL = 'C';
static const char DB_PIXIE_ADDRESS = 'A';
static const char DB_PIXIE_BEST_BLOCK = 'B';

struct PixieGlobalEntry {
    GlobalPixieStats global;
    int activation_height;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(global.total_minted);
        READWRITE(global.next_pixie_id);
        READWRITE(activation_height);
    }
};

struct PixieIdKey {
    char prefix;
    uint32_t pixie_id;

    explicit PixieIdKey(uint32_t id) : prefix(DB_PIXIE_RECORD), pixie_id(id) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prefix);
        READWRITE(pixie_id);
    }
};

struct PixieControlKey {
    char prefix;
    uint256 hash;
    uint32_t n;

    explicit PixieControlKey(const COutPoint& outpoint)
        : prefix(DB_PIXIE_CONTROL), hash(outpoint.hash), n(outpoint.n) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prefix);
        READWRITE(hash);
        READWRITE(n);
    }
};

struct PixieAddressKey {
    char prefix;
    std::string address;

    explicit PixieAddressKey(const std::string& addr) : prefix(DB_PIXIE_ADDRESS), address(addr) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(prefix);
        READWRITE(address);
    }
};

struct PixieRecordDisk {
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

    PixieRecordDisk() : pixie_id(0), world_id(0), palette_id(0), base_type_id(0), traits(0), control_vout(0), height(0) {}

    explicit PixieRecordDisk(const PixieRecord& rec)
        : pixie_id(rec.pixie_id),
          world_id(rec.world_id),
          palette_id(rec.palette_id),
          base_type_id(rec.base_type_id),
          traits(rec.traits),
          pixel_data(rec.pixel_data),
          name(rec.name),
          control_txid(rec.control_txid),
          control_vout(rec.control_vout),
          current_owner(rec.current_owner),
          height(rec.height),
          mint_txid(rec.mint_txid) {}

    PixieRecord ToRecord() const
    {
        PixieRecord rec;
        rec.pixie_id = pixie_id;
        rec.world_id = world_id;
        rec.palette_id = palette_id;
        rec.base_type_id = base_type_id;
        rec.traits = traits;
        rec.pixel_data = pixel_data;
        rec.name = name;
        rec.control_txid = control_txid;
        rec.control_vout = control_vout;
        rec.current_owner = current_owner;
        rec.height = height;
        rec.mint_txid = mint_txid;
        return rec;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(pixie_id);
        READWRITE(world_id);
        READWRITE(palette_id);
        READWRITE(base_type_id);
        READWRITE(traits);
        READWRITE(pixel_data);
        READWRITE(name);
        READWRITE(control_txid);
        READWRITE(control_vout);
        READWRITE(current_owner);
        READWRITE(height);
        READWRITE(mint_txid);
    }
};

struct AddressPixieStatsDisk {
    int last_mint_height;
    uint32_t active_count;

    explicit AddressPixieStatsDisk(const AddressPixieStats& stats)
        : last_mint_height(stats.last_mint_height), active_count(stats.active_count) {}

    AddressPixieStats ToStats() const
    {
        AddressPixieStats stats;
        stats.last_mint_height = last_mint_height;
        stats.active_count = active_count;
        return stats;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(last_mint_height);
        READWRITE(active_count);
    }
};

} // namespace

CPixieDB::CPixieDB(size_t nCacheSize, bool fMemory, bool fWipe)
    : m_cache_size(nCacheSize), m_memory(fMemory)
{
    Reopen(fWipe);
}

void CPixieDB::Reopen(bool fWipe)
{
    m_db = MakeUnique<CDBWrapper>(GetDataDir() / "pixies", m_cache_size, m_memory, fWipe, false);
}

bool CPixieDB::IsEmpty() const
{
    return m_db->IsEmpty();
}

bool CPixieDB::Load(GlobalPixieStats& global,
                    int& activation_height,
                    std::map<uint32_t, PixieRecord>& by_id,
                    std::map<COutPoint, uint32_t>& control_outpoint_to_id,
                    std::map<std::string, AddressPixieStats>& by_address,
                    uint256& best_block) const
{
    if (m_db->IsEmpty())
        return false;

    PixieGlobalEntry global_entry;
    if (!m_db->Read(DB_PIXIE_GLOBAL, global_entry))
        return false;
    global = global_entry.global;
    activation_height = global_entry.activation_height;

    if (!m_db->Read(DB_PIXIE_BEST_BLOCK, best_block))
        return false;

    by_id.clear();
    control_outpoint_to_id.clear();
    by_address.clear();

    std::unique_ptr<CDBIterator> pcursor(m_db->NewIterator());
    for (pcursor->Seek(PixieIdKey(0)); pcursor->Valid(); pcursor->Next()) {
        PixieIdKey key(0);
        if (!pcursor->GetKey(key) || key.prefix != DB_PIXIE_RECORD)
            break;
        PixieRecordDisk disk;
        if (!pcursor->GetValue(disk))
            return false;
        by_id[disk.pixie_id] = disk.ToRecord();
    }

    for (pcursor->Seek(PixieControlKey(COutPoint{})); pcursor->Valid(); pcursor->Next()) {
        PixieControlKey key(COutPoint{});
        if (!pcursor->GetKey(key) || key.prefix != DB_PIXIE_CONTROL)
            break;
        uint32_t pixie_id = 0;
        if (!pcursor->GetValue(pixie_id))
            return false;
        control_outpoint_to_id[COutPoint(key.hash, key.n)] = pixie_id;
    }

    for (pcursor->Seek(PixieAddressKey("")); pcursor->Valid(); pcursor->Next()) {
        PixieAddressKey key("");
        if (!pcursor->GetKey(key) || key.prefix != DB_PIXIE_ADDRESS)
            break;
        AddressPixieStatsDisk disk({0, 0});
        if (!pcursor->GetValue(disk))
            return false;
        by_address[key.address] = disk.ToStats();
    }

    return true;
}

bool CPixieDB::Save(const GlobalPixieStats& global,
                    int activation_height,
                    const std::map<uint32_t, PixieRecord>& by_id,
                    const std::map<COutPoint, uint32_t>& control_outpoint_to_id,
                    const std::map<std::string, AddressPixieStats>& by_address,
                    const uint256& best_block,
                    bool fSync)
{
    m_db.reset();
    Reopen(true);

    CDBBatch batch(*m_db);

    PixieGlobalEntry global_entry;
    global_entry.global = global;
    global_entry.activation_height = activation_height;
    batch.Write(DB_PIXIE_GLOBAL, global_entry);
    batch.Write(DB_PIXIE_BEST_BLOCK, best_block);

    for (const auto& kv : by_id) {
        batch.Write(PixieIdKey(kv.first), PixieRecordDisk(kv.second));
    }
    for (const auto& kv : control_outpoint_to_id) {
        batch.Write(PixieControlKey(kv.first), kv.second);
    }
    for (const auto& kv : by_address) {
        batch.Write(PixieAddressKey(kv.first), AddressPixieStatsDisk(kv.second));
    }

    return m_db->WriteBatch(batch, fSync);
}

bool CPixieDB::ReadBestBlock(uint256& out) const
{
    if (m_db->IsEmpty())
        return false;
    return m_db->Read(DB_PIXIE_BEST_BLOCK, out);
}

} // namespace pixies

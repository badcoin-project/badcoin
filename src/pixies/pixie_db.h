// Copyright (c) 2026 The BadCoin contributors

#ifndef BITCOIN_PIXIES_PIXIE_DB_H
#define BITCOIN_PIXIES_PIXIE_DB_H

#include <dbwrapper.h>
#include <pixies/pixie_index.h>
#include <uint256.h>

#include <map>
#include <memory>
#include <string>

namespace pixies {

/** LevelDB-backed persistence for the pixie index (datadir/pixies/). */
class CPixieDB
{
public:
    explicit CPixieDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool IsEmpty() const;

    bool Load(GlobalPixieStats& global,
              int& activation_height,
              std::map<uint32_t, PixieRecord>& by_id,
              std::map<COutPoint, uint32_t>& control_outpoint_to_id,
              std::map<std::string, AddressPixieStats>& by_address,
              uint256& best_block) const;

    bool Save(const GlobalPixieStats& global,
              int activation_height,
              const std::map<uint32_t, PixieRecord>& by_id,
              const std::map<COutPoint, uint32_t>& control_outpoint_to_id,
              const std::map<std::string, AddressPixieStats>& by_address,
              const uint256& best_block,
              bool fSync = true);

    bool ReadBestBlock(uint256& out) const;

private:
    size_t m_cache_size;
    bool m_memory;
    std::unique_ptr<CDBWrapper> m_db;

    void Reopen(bool fWipe);
};

} // namespace pixies

#endif

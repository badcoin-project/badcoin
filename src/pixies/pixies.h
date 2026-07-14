// Copyright (c) 2026 The BadCoin contributors
// BadPixies on-chain protocol (master spec v0.1, ADR-0005).

#ifndef BITCOIN_PIXIES_PIXIES_H
#define BITCOIN_PIXIES_PIXIES_H

#include <amount.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/standard.h>
#include <uint256.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace pixies {

static const char PIXIE_PROTOCOL_TAG[4] = {'B', 'P', 'X', '1'};
static const uint8_t OP_MINT = 0x01;
static const uint8_t OP_TRANSFER = 0x02;

static const uint32_t PIXIE_GLOBAL_CAP = 21000;
static const uint32_t PIXIE_MAX_ACTIVE_PER_ADDRESS = 10;
static const uint32_t PIXIE_MINT_COOLDOWN_BLOCKS = 1440; // 24h at 60s block spacing
static const size_t PIXIE_PIXEL_DATA_LEN = 288;
static const size_t PIXIE_MINT_PAYLOAD_MIN = 303;
static const size_t PIXIE_MINT_PAYLOAD_MAX = 335;
static const CAmount PIXIE_CONTROL_UTXO_VALUE = 546;

/** Block subsidy 2,137 BAD — mint burns are multiples of this (ADR-0007 / badpixies). */
static const CAmount PIXIE_BLOCK_SUBSIDY = 213700000000;       // 1 block
static const CAmount PIXIE_BURN_FOUNDING = PIXIE_BLOCK_SUBSIDY; // 1 block
static const CAmount PIXIE_BURN_EARLY = 5342500000000;         // 25 blocks (53,425 BAD)
static const CAmount PIXIE_BURN_STANDARD = 21370000000000;     // 100 blocks (213,700 BAD)

enum class PixieRejectCode {
    OK,
    MALFORMED,
    TRAITS,
    NAME,
    SUPPLY_CAP,
    COOLDOWN,
    ACTIVE_MINT_CAP,
    BURN,
    TRANSFER_NOT_FOUND,
    TRANSFER_ACTIVE_CAP,
    NOT_ACTIVE,
};

struct PixieMintData {
    uint8_t world_id;
    uint8_t palette_id;
    uint8_t base_type_id;
    uint32_t traits;
    std::vector<uint8_t> pixel_data;
    std::string name;
};

struct PixieTransferData {
    uint32_t pixie_id;
};

/** Extract concatenated pushdata after OP_RETURN. */
bool ExtractOpReturnPayload(const CScript& script, std::vector<uint8_t>& payload);

bool ParseMintPayload(const std::vector<uint8_t>& payload, PixieMintData& out);
bool ParseTransferPayload(const std::vector<uint8_t>& payload, PixieTransferData& out);

bool BuildMintPayload(const PixieMintData& mint, std::vector<uint8_t>& out);
bool BuildTransferPayload(uint32_t pixie_id, std::vector<uint8_t>& out);
bool BuildMintOpReturnScript(const PixieMintData& mint, CScript& script);
CScript BuildPixieBurnScript(CAmount burn_sats);

bool ValidateMintTraits(uint32_t traits);
bool ValidateMintName(const std::string& name, PixieRejectCode& code);
CAmount ExpectedMintBurnSats(uint32_t next_pixie_id);

/** Find 546-sat P2PKH/P2SH control output; returns vout index or -1. */
int FindControlUtxoVout(const CTransaction& tx);

/** Expected burn output script for tier (BPIX + 8 byte LE cost). */
bool FindMintBurnOutput(const CTransaction& tx, CAmount expected_sats, bool burn_optional);

std::string PixieRejectCodeString(PixieRejectCode code);

} // namespace pixies

#endif

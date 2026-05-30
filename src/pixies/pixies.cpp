// Copyright (c) 2026 The BadCoin contributors

#include <pixies/pixies.h>

#include <policy/policy.h>
#include <util.h>
#include <utilstrencodings.h>

#include <algorithm>

namespace pixies {

bool ExtractOpReturnPayload(const CScript& script, std::vector<uint8_t>& payload)
{
    payload.clear();
    if (script.size() < 2 || script[0] != OP_RETURN)
        return false;

    CScript::const_iterator pc = script.begin() + 1;
    opcodetype opcode;
    while (pc < script.end()) {
        std::vector<unsigned char> vch;
        if (!script.GetOp(pc, opcode, vch))
            return false;
        payload.insert(payload.end(), vch.begin(), vch.end());
    }
    return !payload.empty();
}

static bool ReadU32LE(const std::vector<uint8_t>& b, size_t off, uint32_t& v)
{
    if (off + 4 > b.size()) return false;
    v = uint32_t(b[off]) | (uint32_t(b[off + 1]) << 8) | (uint32_t(b[off + 2]) << 16) |
        (uint32_t(b[off + 3]) << 24);
    return true;
}

static bool ReadU16LE(const std::vector<uint8_t>& b, size_t off, uint16_t& v)
{
    if (off + 2 > b.size()) return false;
    v = uint16_t(b[off]) | (uint16_t(b[off + 1]) << 8);
    return true;
}

bool ParseMintPayload(const std::vector<uint8_t>& payload, PixieMintData& out)
{
    if (payload.size() < PIXIE_MINT_PAYLOAD_MIN || payload.size() > PIXIE_MINT_PAYLOAD_MAX)
        return false;
    if (payload.size() < 4 || memcmp(payload.data(), PIXIE_PROTOCOL_TAG, 4) != 0)
        return false;
    if (payload[4] != OP_MINT)
        return false;

    out.world_id = payload[5];
    out.palette_id = payload[6];
    out.base_type_id = payload[7];
    if (!ReadU32LE(payload, 8, out.traits))
        return false;

    uint16_t pixel_len;
    if (!ReadU16LE(payload, 12, pixel_len) || pixel_len != PIXIE_PIXEL_DATA_LEN)
        return false;

    if (payload.size() < 14 + PIXIE_PIXEL_DATA_LEN + 1)
        return false;

    out.pixel_data.assign(payload.begin() + 14, payload.begin() + 14 + PIXIE_PIXEL_DATA_LEN);

    uint8_t name_len = payload[14 + PIXIE_PIXEL_DATA_LEN];
    if (14 + PIXIE_PIXEL_DATA_LEN + 1 + name_len != payload.size())
        return false;

    out.name.assign(reinterpret_cast<const char*>(payload.data() + 14 + PIXIE_PIXEL_DATA_LEN + 1), name_len);
    return true;
}

static void WriteU32LE(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 24));
}

static void WriteU16LE(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
}

bool BuildMintPayload(const PixieMintData& mint, std::vector<uint8_t>& out)
{
    out.clear();
    if (mint.pixel_data.size() != PIXIE_PIXEL_DATA_LEN)
        return false;
    if (mint.world_id > 7 || mint.palette_id > 4 || mint.base_type_id > 3)
        return false;
    if (!ValidateMintTraits(mint.traits))
        return false;
    PixieRejectCode code;
    if (!ValidateMintName(mint.name, code))
        return false;

    out.insert(out.end(), PIXIE_PROTOCOL_TAG, PIXIE_PROTOCOL_TAG + 4);
    out.push_back(OP_MINT);
    out.push_back(mint.world_id);
    out.push_back(mint.palette_id);
    out.push_back(mint.base_type_id);
    WriteU32LE(out, mint.traits);
    WriteU16LE(out, static_cast<uint16_t>(PIXIE_PIXEL_DATA_LEN));
    out.insert(out.end(), mint.pixel_data.begin(), mint.pixel_data.end());
    if (mint.name.size() > 255)
        return false;
    out.push_back(static_cast<uint8_t>(mint.name.size()));
    out.insert(out.end(), mint.name.begin(), mint.name.end());

    if (out.size() < PIXIE_MINT_PAYLOAD_MIN || out.size() > PIXIE_MINT_PAYLOAD_MAX)
        return false;
    return true;
}

bool BuildTransferPayload(uint32_t pixie_id, std::vector<uint8_t>& out)
{
    out.clear();
    out.insert(out.end(), PIXIE_PROTOCOL_TAG, PIXIE_PROTOCOL_TAG + 4);
    out.push_back(OP_TRANSFER);
    WriteU32LE(out, pixie_id);
    return out.size() == 9;
}

bool BuildMintOpReturnScript(const PixieMintData& mint, CScript& script)
{
    std::vector<uint8_t> payload;
    if (!BuildMintPayload(mint, payload))
        return false;
    script = CScript() << OP_RETURN << payload;
    return true;
}

CScript BuildPixieBurnScript(CAmount burn_sats)
{
    std::vector<unsigned char> data(12);
    memcpy(data.data(), "BPIX", 4);
    uint64_t cost = static_cast<uint64_t>(burn_sats);
    for (int i = 0; i < 8; ++i)
        data[4 + i] = static_cast<unsigned char>((cost >> (8 * i)) & 0xff);
    return CScript() << OP_RETURN << data;
}

bool ValidateMintTraits(uint32_t traits)
{
    return (traits & 0x00000300) == 0 && (traits & 0xFFC00000) == 0;
}

bool ParseTransferPayload(const std::vector<uint8_t>& payload, PixieTransferData& out)
{
    if (payload.size() != 9)
        return false;
    if (memcmp(payload.data(), PIXIE_PROTOCOL_TAG, 4) != 0)
        return false;
    if (payload[4] != OP_TRANSFER)
        return false;
    return ReadU32LE(payload, 5, out.pixie_id);
}

bool ValidateMintName(const std::string& name, PixieRejectCode& code)
{
    code = PixieRejectCode::OK;
    if (name.size() > 32)
        return false;
    for (size_t i = 0; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (c < 0x20 && c != 0x09)
            return code = PixieRejectCode::NAME, false;
        if (c == 0x7F || (c >= 0x80 && c <= 0x9F))
            return code = PixieRejectCode::NAME, false;
        // Basic UTF-8 validity: reject obviously invalid lead bytes only for v1
        if (c >= 0xF5)
            return code = PixieRejectCode::NAME, false;
    }
    return true;
}

CAmount ExpectedMintBurnSats(uint32_t next_pixie_id)
{
    if (next_pixie_id <= 100)
        return 0;
    if (next_pixie_id <= 1100)
        return PIXIE_BURN_EARLY;
    return PIXIE_BURN_STANDARD;
}

int FindControlUtxoVout(const CTransaction& tx)
{
    for (size_t i = 0; i < tx.vout.size(); ++i) {
        const CTxOut& out = tx.vout[i];
        if (out.nValue == PIXIE_CONTROL_UTXO_VALUE && !out.scriptPubKey.empty() &&
            out.scriptPubKey[0] != OP_RETURN) {
            txnouttype t;
            if (IsStandard(out.scriptPubKey, t, false))
                return static_cast<int>(i);
        }
    }
    return -1;
}

static bool ScriptIsPixieBurn(const CScript& script, CAmount expected_sats)
{
    if (script.size() < 2 || script[0] != OP_RETURN)
        return false;
    std::vector<uint8_t> payload;
    if (!ExtractOpReturnPayload(script, payload))
        return false;
    if (payload.size() != 12)
        return false;
    if (memcmp(payload.data(), "BPIX", 4) != 0)
        return false;
    uint64_t cost = uint64_t(payload[4]) | (uint64_t(payload[5]) << 8) | (uint64_t(payload[6]) << 16) |
                    (uint64_t(payload[7]) << 24) | (uint64_t(payload[8]) << 32) |
                    (uint64_t(payload[9]) << 40) | (uint64_t(payload[10]) << 48) |
                    (uint64_t(payload[11]) << 56);
    return static_cast<CAmount>(cost) == expected_sats;
}

bool FindMintBurnOutput(const CTransaction& tx, CAmount expected_sats, bool founding_optional)
{
    if (expected_sats == 0 && founding_optional) {
        for (const CTxOut& out : tx.vout) {
            if (out.scriptPubKey.size() >= 2 && out.scriptPubKey[0] == OP_RETURN &&
                ScriptIsPixieBurn(out.scriptPubKey, 0))
                return true;
        }
        return true; // burn optional for founding
    }
    for (const CTxOut& out : tx.vout) {
        if (ScriptIsPixieBurn(out.scriptPubKey, expected_sats))
            return true;
    }
    return false;
}

std::string PixieRejectCodeString(PixieRejectCode code)
{
    switch (code) {
    case PixieRejectCode::OK: return "ok";
    case PixieRejectCode::MALFORMED: return "PIXIE_MINT_MALFORMED";
    case PixieRejectCode::TRAITS: return "PIXIE_MINT_TRAITS";
    case PixieRejectCode::NAME: return "PIXIE_MINT_NAME";
    case PixieRejectCode::SUPPLY_CAP: return "PIXIE_MINT_SUPPLY_CAP";
    case PixieRejectCode::COOLDOWN: return "PIXIE_MINT_COOLDOWN";
    case PixieRejectCode::ACTIVE_MINT_CAP: return "PIXIE_MINT_ACTIVE_CAP";
    case PixieRejectCode::BURN: return "PIXIE_MINT_BURN";
    case PixieRejectCode::TRANSFER_NOT_FOUND: return "PIXIE_TRANSFER_NOT_FOUND";
    case PixieRejectCode::TRANSFER_ACTIVE_CAP: return "PIXIE_TRANSFER_ACTIVE_CAP";
    case PixieRejectCode::NOT_ACTIVE: return "PIXIE_NOT_ACTIVE";
    }
    return "PIXIE_UNKNOWN";
}

} // namespace pixies

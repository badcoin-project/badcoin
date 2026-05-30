// Copyright (c) 2026 The BadCoin contributors

#include <pixies/pixie_index.h>
#include <pixies/pixies.h>
#include <core_io.h>
#include <base58.h>
#include <chainparams.h>
#include <rpc/server.h>
#include <validation.h>

#include <algorithm>
#include <rpc/util.h>
#include <univalue.h>
#include <util.h>
#include <utilstrencodings.h>

static uint32_t ParamUInt32(const UniValue& v, const char* name)
{
    if (v.isNum())
        return static_cast<uint32_t>(v.get_int64());
    if (v.isStr())
        return static_cast<uint32_t>(atoi64(v.get_str()));
    throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Expected number for ") + name);
}

static UniValue PixieRecordToJSON(const pixies::PixieRecord& rec)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("pixie_id", static_cast<int>(rec.pixie_id));
    obj.pushKV("world_id", rec.world_id);
    obj.pushKV("palette_id", rec.palette_id);
    obj.pushKV("base_type_id", rec.base_type_id);
    obj.pushKV("traits", static_cast<int>(rec.traits));
    obj.pushKV("pixel_data", HexStr(rec.pixel_data.begin(), rec.pixel_data.end()));
    obj.pushKV("name", rec.name);
    obj.pushKV("current_owner", rec.current_owner);
    obj.pushKV("mint_height", rec.height);
    obj.pushKV("mint_txid", rec.mint_txid.GetHex());
    obj.pushKV("control_txid", rec.control_txid.GetHex());
    obj.pushKV("control_vout", static_cast<int>(rec.control_vout));
    return obj;
}

static UniValue getpixie(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error("getpixie pixie_id\n");

    uint32_t id = ParamUInt32(request.params[0], "pixie_id");

    pixies::PixieRecord rec;
    if (!pixies::CPixieIndex::Instance().GetPixie(id, rec))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Pixie not found");

    return PixieRecordToJSON(rec);
}

static UniValue getpixieowner(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error("getpixieowner pixie_id\n");

    uint32_t id = ParamUInt32(request.params[0], "pixie_id");

    pixies::PixieRecord rec;
    if (!pixies::CPixieIndex::Instance().GetPixie(id, rec))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Pixie not found");

    return rec.current_owner;
}

static UniValue getpixiestats(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error("getpixiestats\n");

    pixies::GlobalPixieStats s = pixies::CPixieIndex::Instance().GetGlobalStats();
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("total_minted", static_cast<int>(s.total_minted));
    obj.pushKV("cap", static_cast<int>(pixies::PIXIE_GLOBAL_CAP));
    obj.pushKV("next_pixie_id", static_cast<int>(s.next_pixie_id));
    const int founding_remaining = std::max(0, 100 - static_cast<int>(s.total_minted));
    const int early_remaining = std::max(0, 1100 - static_cast<int>(s.total_minted));
    const int standard_remaining = std::max(0, 21000 - static_cast<int>(s.total_minted));
    obj.pushKV("founding_remaining", founding_remaining);
    obj.pushKV("early_remaining", early_remaining);
    obj.pushKV("standard_remaining", standard_remaining);
    return obj;
}

static UniValue listpixies(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw std::runtime_error("listpixies address (start) (count)\n");

    RPCTypeCheck(request.params, {UniValue::VSTR});

    std::string address = request.params[0].get_str();
    std::vector<pixies::PixieRecord> all = pixies::CPixieIndex::Instance().ListByAddress(address);

    size_t start = 0;
    size_t count = all.size();
    if (request.params.size() > 1)
        start = static_cast<size_t>(request.params[1].get_int());
    if (request.params.size() > 2)
        count = static_cast<size_t>(request.params[2].get_int());

    UniValue arr(UniValue::VARR);
    for (size_t i = start; i < all.size() && i < start + count; ++i)
        arr.push_back(PixieRecordToJSON(all[i]));
    return arr;
}

static UniValue decodepixiepayload(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error("decodepixiepayload op_return_hex\n");

    std::vector<unsigned char> data = ParseHex(request.params[0].get_str());
    std::vector<uint8_t> payload(data.begin(), data.end());

    UniValue obj(UniValue::VOBJ);
    if (payload.size() >= 5 && memcmp(payload.data(), pixies::PIXIE_PROTOCOL_TAG, 4) == 0) {
        if (payload[4] == pixies::OP_MINT) {
            pixies::PixieMintData mint;
            if (pixies::ParseMintPayload(payload, mint)) {
                obj.pushKV("op", "MINT");
                obj.pushKV("world_id", mint.world_id);
                obj.pushKV("palette_id", mint.palette_id);
                obj.pushKV("base_type_id", mint.base_type_id);
                obj.pushKV("traits", static_cast<int>(mint.traits));
                obj.pushKV("pixel_data", HexStr(mint.pixel_data.begin(), mint.pixel_data.end()));
                obj.pushKV("name", mint.name);
                return obj;
            }
        } else if (payload[4] == pixies::OP_TRANSFER) {
            pixies::PixieTransferData xfer;
            if (pixies::ParseTransferPayload(payload, xfer)) {
                obj.pushKV("op", "TRANSFER");
                obj.pushKV("pixie_id", static_cast<int>(xfer.pixie_id));
                return obj;
            }
        }
    }
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid pixie payload");
}

static CMutableTransaction BuildPixieMintTx(const CTxDestination& control_dest, const pixies::PixieMintData& mint)
{
    CMutableTransaction tx;
    CScript mintScript;
    if (!pixies::BuildMintOpReturnScript(mint, mintScript))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mint fields");

    tx.vout.emplace_back(0, mintScript);
    tx.vout.emplace_back(pixies::PIXIE_CONTROL_UTXO_VALUE, GetScriptForDestination(control_dest));

    const pixies::GlobalPixieStats stats = pixies::CPixieIndex::Instance().GetGlobalStats();
    const CAmount burn = pixies::ExpectedMintBurnSats(stats.next_pixie_id);
    if (burn > 0)
        tx.vout.emplace_back(0, pixies::BuildPixieBurnScript(burn));

    return tx;
}

static UniValue createpixiemint(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 6 || request.params.size() > 7)
        throw std::runtime_error(
            "createpixiemint \"control_address\" world_id palette_id base_type_id traits hex_pixel_data (name)\n"
            "\nBuild an unsigned mint transaction (vout[0] = BPX1 MINT OP_RETURN).\n"
            "Fund with fundrawtransaction, then sign and broadcast.\n"
            "\nArguments:\n"
            "1. control_address   (string) P2PKH/P2SH address receiving the 546-sat control UTXO\n"
            "2. world_id          (numeric)\n"
            "3. palette_id        (numeric)\n"
            "4. base_type_id      (numeric)\n"
            "5. traits            (numeric) little-endian trait bitfield\n"
            "6. hex_pixel_data     (string) 288 bytes hex (576 characters)\n"
            "7. name              (string, optional) UTF-8, max 32 bytes\n");

    const CTxDestination control_dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(control_dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid control address");

    pixies::PixieMintData mint;
    auto param_int = [](const UniValue& v) -> int64_t {
        if (v.isNum())
            return v.get_int64();
        if (v.isStr())
            return atoi64(v.get_str());
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expected number");
    };
    mint.world_id = static_cast<uint8_t>(param_int(request.params[1]));
    mint.palette_id = static_cast<uint8_t>(param_int(request.params[2]));
    mint.base_type_id = static_cast<uint8_t>(param_int(request.params[3]));
    mint.traits = static_cast<uint32_t>(param_int(request.params[4]));

    const std::vector<unsigned char> pixel = ParseHex(request.params[5].get_str());
    if (pixel.size() != pixies::PIXIE_PIXEL_DATA_LEN)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "hex_pixel_data must be exactly 288 bytes");

    mint.pixel_data.assign(pixel.begin(), pixel.end());
    if (request.params.size() > 6 && !request.params[6].isNull())
        mint.name = request.params[6].get_str();

    pixies::PixieRejectCode code;
    if (!pixies::ValidateMintName(mint.name, code))
        throw JSONRPCError(RPC_INVALID_PARAMETER, pixies::PixieRejectCodeString(code));
    if (!pixies::ValidateMintTraits(mint.traits))
        throw JSONRPCError(RPC_INVALID_PARAMETER, pixies::PixieRejectCodeString(pixies::PixieRejectCode::TRAITS));

    const CMutableTransaction tx = BuildPixieMintTx(control_dest, mint);
    return EncodeHexTx(CTransaction(tx));
}

static UniValue createpixietransfer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "createpixietransfer pixie_id \"to_address\"\n"
            "\nBuild an unsigned transfer (spends control UTXO; vout[0] = BPX1 TRANSFER OP_RETURN).\n");

    const uint32_t pixie_id = ParamUInt32(request.params[0], "pixie_id");
    const CTxDestination to_dest = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(to_dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid destination address");

    pixies::PixieRecord rec;
    if (!pixies::CPixieIndex::Instance().GetPixie(pixie_id, rec))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Pixie not found");

    std::vector<uint8_t> xfer_payload;
    if (!pixies::BuildTransferPayload(pixie_id, xfer_payload))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to build transfer payload");

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(rec.control_txid, rec.control_vout));
    tx.vout.emplace_back(0, CScript() << OP_RETURN << xfer_payload);
    tx.vout.emplace_back(pixies::PIXIE_CONTROL_UTXO_VALUE, GetScriptForDestination(to_dest));

    return EncodeHexTx(CTransaction(tx));
}

static UniValue getpixelmintcost(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error("getpixelmintcost\n");

    const pixies::GlobalPixieStats stats = pixies::CPixieIndex::Instance().GetGlobalStats();
    return ValueFromAmount(pixies::ExpectedMintBurnSats(stats.next_pixie_id));
}

static UniValue getpixieactivecount(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error("getpixieactivecount \"address\"\n");

    RPCTypeCheck(request.params, {UniValue::VSTR});
    const std::string address = request.params[0].get_str();
    if (!IsValidDestinationString(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    return static_cast<int>(pixies::CPixieIndex::Instance().GetActiveCount(address));
}

static UniValue getpixiecooldown(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error("getpixiecooldown \"address\"\n");

    RPCTypeCheck(request.params, {UniValue::VSTR});
    const std::string address = request.params[0].get_str();
    if (!IsValidDestinationString(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");

    int height = 0;
    {
        LOCK(cs_main);
        if (chainActive.Tip())
            height = chainActive.Height();
    }

    const int blocks = pixies::CPixieIndex::Instance().GetMintCooldownBlocksRemaining(address, height);
    if (blocks < 0)
        return NullUniValue;

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("blocks_remaining", blocks);
    obj.pushKV("seconds_estimate", blocks * 60);
    return obj;
}

static UniValue listallpixies(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2)
        throw std::runtime_error("listallpixies (start) (count)\n");

    std::vector<pixies::PixieRecord> all = pixies::CPixieIndex::Instance().ListAll();

    size_t start = 0;
    size_t count = all.size();
    if (request.params.size() > 0 && !request.params[0].isNull())
        start = static_cast<size_t>(ParamUInt32(request.params[0], "start"));
    if (request.params.size() > 1 && !request.params[1].isNull())
        count = static_cast<size_t>(ParamUInt32(request.params[1], "count"));

    UniValue arr(UniValue::VARR);
    for (size_t i = start; i < all.size() && i < start + count; ++i)
        arr.push_back(PixieRecordToJSON(all[i]));
    return arr;
}

static UniValue setpixieteststats(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "setpixieteststats total_minted next_pixie_id\n"
            "\nRegtest harness only: set global pixie counters without creating records.\n");

    if (Params().NetworkIDString() != "regtest")
        throw JSONRPCError(RPC_MISC_ERROR, "setpixieteststats is regtest-only");

    const uint32_t total = ParamUInt32(request.params[0], "total_minted");
    const uint32_t next_id = ParamUInt32(request.params[1], "next_pixie_id");
    pixies::CPixieIndex::Instance().SetTestGlobalStats(total, next_id);
    return NullUniValue;
}

static const CRPCCommand commands[] = {
    {"pixies", "getpixie", &getpixie, {"pixie_id"}},
    {"pixies", "getpixieowner", &getpixieowner, {"pixie_id"}},
    {"pixies", "getpixiestats", &getpixiestats, {}},
    {"pixies", "listpixies", &listpixies, {"address", "start", "count"}},
    {"pixies", "decodepixiepayload", &decodepixiepayload, {"hex"}},
    {"pixies", "createpixiemint", &createpixiemint,
        {"control_address", "world_id", "palette_id", "base_type_id", "traits", "hex_pixel_data", "name"}},
    {"pixies", "createpixietransfer", &createpixietransfer, {"pixie_id", "to_address"}},
    {"pixies", "getpixelmintcost", &getpixelmintcost, {}},
    {"pixies", "getpixieactivecount", &getpixieactivecount, {"address"}},
    {"pixies", "getpixiecooldown", &getpixiecooldown, {"address"}},
    {"pixies", "listallpixies", &listallpixies, {"start", "count"}},
    {"hidden", "setpixieteststats", &setpixieteststats, {"total_minted", "next_pixie_id"}},
};

void RegisterPixiesRPCCommands(CRPCTable& t)
{
    for (unsigned int i = 0; i < ARRAYLEN(commands); ++i)
        t.appendCommand(commands[i].name, &commands[i]);
}

// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <chain.h>
#include <clientversion.h>
#include <core_io.h>
#include <crypto/ripemd160.h>
#include <init.h>
#include <validation.h>
#include <httpserver.h>
#include <net.h>
#include <netbase.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <timedata.h>
#include <util.h>
#include <utilstrencodings.h>
#ifdef ENABLE_WALLET
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#endif
#include <warnings.h>

#include <stdint.h>
#ifdef HAVE_MALLOC_INFO
#include <malloc.h>
#endif

#include <univalue.h>

#ifdef ENABLE_WALLET
class DescribeAddressVisitor : public boost::static_visitor<UniValue>
{
public:
    CWallet* const pwallet;

    explicit DescribeAddressVisitor(CWallet* _pwallet) : pwallet(_pwallet) {}

    void ProcessSubScript(const CScript& subscript, UniValue& obj, bool include_addresses = false) const
    {
        // Always present: script type and redeemscript
        txnouttype which_type;
        std::vector<std::vector<unsigned char>> solutions_data;
        Solver(subscript, which_type, solutions_data);
        obj.pushKV("script", GetTxnOutputType(which_type));
        obj.pushKV("hex", HexStr(subscript.begin(), subscript.end()));

        CTxDestination embedded;
        UniValue a(UniValue::VARR);
        if (ExtractDestination(subscript, embedded)) {
            // Only when the script corresponds to an address.
            UniValue subobj = boost::apply_visitor(*this, embedded);
            subobj.pushKV("address", EncodeDestination(embedded));
            subobj.pushKV("scriptPubKey", HexStr(subscript.begin(), subscript.end()));
            // Always report the pubkey at the top level, so that `getnewaddress()['pubkey']` always works.
            if (subobj.exists("pubkey")) obj.pushKV("pubkey", subobj["pubkey"]);
            obj.pushKV("embedded", std::move(subobj));
            if (include_addresses) a.push_back(EncodeDestination(embedded));
        } else if (which_type == TX_MULTISIG) {
            // Also report some information on multisig scripts (which do not have a corresponding address).
            // TODO: abstract out the common functionality between this logic and ExtractDestinations.
            obj.pushKV("sigsrequired", solutions_data[0][0]);
            UniValue pubkeys(UniValue::VARR);
            for (size_t i = 1; i < solutions_data.size() - 1; ++i) {
                CPubKey key(solutions_data[i].begin(), solutions_data[i].end());
                if (include_addresses) a.push_back(EncodeDestination(key.GetID()));
                pubkeys.push_back(HexStr(key.begin(), key.end()));
            }
            obj.pushKV("pubkeys", std::move(pubkeys));
        }

        // The "addresses" field is confusing because it refers to public keys using their P2PKH address.
        // For that reason, only add the 'addresses' field when needed for backward compatibility. New applications
        // can use the 'embedded'->'address' field for P2SH or P2WSH wrapped addresses, and 'pubkeys' for
        // inspecting multisig participants.
        if (include_addresses) obj.pushKV("addresses", std::move(a));
    }

    UniValue operator()(const CNoDestination& dest) const { return UniValue(UniValue::VOBJ); }

    UniValue operator()(const CKeyID& keyID) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey vchPubKey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", false));
        if (pwallet && pwallet->GetPubKey(keyID, vchPubKey)) {
            obj.push_back(Pair("pubkey", HexStr(vchPubKey)));
            obj.push_back(Pair("iscompressed", vchPubKey.IsCompressed()));
        }
        return obj;
    }

    UniValue operator()(const CScriptID& scriptID) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", false));
        if (pwallet && pwallet->GetCScript(scriptID, subscript)) {
            ProcessSubScript(subscript, obj, true);
        }
        return obj;
    }

    UniValue operator()(const WitnessV0KeyHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CPubKey pubkey;
        obj.push_back(Pair("isscript", false));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        if (pwallet && pwallet->GetPubKey(CKeyID(id), pubkey)) {
            obj.push_back(Pair("pubkey", HexStr(pubkey)));
        }
        return obj;
    }

    UniValue operator()(const WitnessV0ScriptHash& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("isscript", true));
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", 0));
        obj.push_back(Pair("witness_program", HexStr(id.begin(), id.end())));
        CRIPEMD160 hasher;
        uint160 hash;
        hasher.Write(id.begin(), 32).Finalize(hash.begin());
        if (pwallet && pwallet->GetCScript(CScriptID(hash), subscript)) {
            ProcessSubScript(subscript, obj);
        }
        return obj;
    }

    UniValue operator()(const WitnessUnknown& id) const
    {
        UniValue obj(UniValue::VOBJ);
        CScript subscript;
        obj.push_back(Pair("iswitness", true));
        obj.push_back(Pair("witness_version", (int)id.version));
        obj.push_back(Pair("witness_program", HexStr(id.program, id.program + id.length)));
        return obj;
    }
};
#endif

UniValue validateaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "validateaddress \"address\"\n"
            "\nReturn information about the given Badcoin address.\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The Badcoin address to validate.\n"
            "\nResult:\n"
            "{\n"
            "  \"isvalid\" : true|false,       (boolean) Whether the address is valid. If false, this is the only field returned.\n"
            "  \"address\" : \"address\",        (string) The Badcoin address validated.\n"
            "  \"scriptPubKey\" : \"hex\",       (string) The hex-encoded scriptPubKey generated by the address.\n"
            "  \"ismine\" : true|false,        (boolean) Whether the address belongs to this wallet.\n"
            "  \"iswatchonly\" : true|false,   (boolean) Whether the address is watch-only.\n"
            "  \"isscript\" : true|false,      (boolean, optional) True if the address is P2SH or P2WSH. Not included for unknown witness types.\n"
            "  \"iswitness\" : true|false,     (boolean) True if the address is P2WPKH, P2WSH, or an unknown witness version.\n"
            "  \"witness_version\" : n         (numeric, optional) Witness version for witness output types.\n"
            "  \"witness_program\" : \"hex\"     (string, optional) Witness program (script or key hash) for witness output types.\n"
            "  \"script\" : \"type\"             (string, optional) The output script type. Only if \"isscript\" is true and the redeemscript is known.\n"
            "  \"hex\" : \"hex\",                (string, optional) The redeemscript for P2SH or P2WSH addresses.\n"
            "  \"addresses\" : [                (array, optional) Addresses associated with a known redeemscript (non-witness only).\n"
            "      \"address\"                  (string)\n"
            "      ,...\n"
            "    ]\n"
            "  \"pubkeys\" : [                  (array, optional) Pubkeys for multisig redeemscripts (\"script\" == \"multisig\").\n"
            "      \"pubkey\"                   (string)\n"
            "      ,...\n"
            "    ]\n"
            "  \"sigsrequired\" : n             (numeric, optional) Number of signatures required for a multisig output.\n"
            "  \"pubkey\" : \"hex\",             (string, optional) Raw public key for single-key addresses (possibly embedded in P2SH or P2WSH).\n"
            "  \"embedded\" : {...},           (object, optional) Information about the address embedded inside P2SH or P2WSH.\n"
            "  \"iscompressed\" : true|false,  (boolean, optional) Whether the public key is compressed.\n"
            "  \"account\" : \"name\",           (string, optional) The legacy account/label associated with the address.\n"
            "  \"timestamp\" : timestamp,      (numeric, optional) Key creation time (seconds since Unix epoch), if known.\n"
            "  \"hdkeypath\" : \"path\",         (string, optional) HD key path, if the key is HD and available.\n"
            "  \"hdmasterkeyid\" : \"<hash160>\" (string, optional) Hash160 of the HD master public key.\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("validateaddress", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\"")
            + HelpExampleRpc("validateaddress", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\"")
        );

#ifdef ENABLE_WALLET
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);

    LOCK2(cs_main, pwallet ? &pwallet->cs_wallet : nullptr);
#else
    LOCK(cs_main);
#endif

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    bool isValid = IsValidDestination(dest);

    UniValue ret(UniValue::VOBJ);
    ret.push_back(Pair("isvalid", isValid));
    if (isValid) {
        std::string currentAddress = EncodeDestination(dest);
        ret.push_back(Pair("address", currentAddress));

        CScript scriptPubKey = GetScriptForDestination(dest);
        ret.push_back(Pair("scriptPubKey", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

#ifdef ENABLE_WALLET
        isminetype mine = pwallet ? IsMine(*pwallet, dest) : ISMINE_NO;
        ret.push_back(Pair("ismine", bool(mine & ISMINE_SPENDABLE)));
        ret.push_back(Pair("iswatchonly", bool(mine & ISMINE_WATCH_ONLY)));
        UniValue detail = boost::apply_visitor(DescribeAddressVisitor(pwallet), dest);
        ret.pushKVs(detail);
        if (pwallet && pwallet->mapAddressBook.count(dest)) {
            ret.push_back(Pair("account", pwallet->mapAddressBook[dest].name));
        }
        if (pwallet) {
            const CKeyMetadata* meta = nullptr;
            CKeyID key_id = GetKeyForDestination(*pwallet, dest);
            if (!key_id.IsNull()) {
                auto it = pwallet->mapKeyMetadata.find(key_id);
                if (it != pwallet->mapKeyMetadata.end()) {
                    meta = &it->second;
                }
            }
            if (!meta) {
                auto it = pwallet->m_script_metadata.find(CScriptID(scriptPubKey));
                if (it != pwallet->m_script_metadata.end()) {
                    meta = &it->second;
                }
            }
            if (meta) {
                ret.push_back(Pair("timestamp", meta->nCreateTime));
                if (!meta->hdKeypath.empty()) {
                    ret.push_back(Pair("hdkeypath", meta->hdKeypath));
                    ret.push_back(Pair("hdmasterkeyid", meta->hdMasterKeyID.GetHex()));
                }
            }
        }
#endif
    }
    return ret;
}

// Needed even with !ENABLE_WALLET, to pass (ignored) pointers around
class CWallet;

UniValue createmultisig(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 2) {
        std::string msg =
            "createmultisig nrequired [\"key\",...]\n"
            "\nCreate a multisignature address with *n* required signatures out of the provided keys.\n"
            "Returns a JSON object with the multisig address and redeemScript.\n"
            "\nUsing addresses with createmultisig is deprecated. It is recommended to pass hex-encoded\n"
            "public keys. If you must use addresses, start badcoind with -deprecatedrpc=createmultisig\n"
            "and ensure the addresses are known to the wallet (consider addmultisigaddress).\n"
            "\nArguments:\n"
            "1. nrequired                    (numeric, required) Number of required signatures out of the keys.\n"
            "2. \"keys\"                       (array, required) JSON array of hex-encoded public keys (or, if\n"
            "                                  deprecated support is enabled, addresses known to the wallet).\n"
            "     [\n"
            "       \"key\"                    (string) Hex-encoded public key\n"
            "       ,...\n"
            "     ]\n"
            "\nResult:\n"
            "{\n"
            "  \"address\" : \"multisigaddress\", (string) The new multisignature address.\n"
            "  \"redeemScript\" : \"script\"      (string) Hex-encoded redemption script.\n"
            "}\n"
            "\nExample:\n"
            "Create a 2-of-2 multisig from two public keys\n"
            + HelpExampleCli("createmultisig", "2 \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("createmultisig", "2, \"[\\\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\\\",\\\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\\\"]\"");

        throw std::runtime_error(msg);
    }

    int required = request.params[0].get_int();

    // Get the public keys
    const UniValue& keys = request.params[1].get_array();
    std::vector<CPubKey> pubkeys;
    for (unsigned int i = 0; i < keys.size(); ++i) {
        if (IsHex(keys[i].get_str()) &&
            (keys[i].get_str().length() == 66 || keys[i].get_str().length() == 130)) {
            pubkeys.push_back(HexToPubKey(keys[i].get_str()));
        } else {
#ifdef ENABLE_WALLET
            CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
            if (IsDeprecatedRPCEnabled("createmultisig") && EnsureWalletIsAvailable(pwallet, false)) {
                pubkeys.push_back(AddrToPubKey(pwallet, keys[i].get_str()));
            } else
#endif
                throw JSONRPCError(
                    RPC_INVALID_ADDRESS_OR_KEY,
                    strprintf("Invalid public key: %s\n"
                              "createmultisig expects hex-encoded public keys. Using addresses is "
                              "deprecated and only available with -deprecatedrpc=createmultisig.",
                              keys[i].get_str()));
        }
    }

    // Construct using pay-to-script-hash:
    CScript inner = CreateMultisigRedeemscript(required, pubkeys);
    CScriptID innerID(inner);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("address", EncodeDestination(innerID)));
    result.push_back(Pair("redeemScript", HexStr(inner.begin(), inner.end())));

    return result;
}

UniValue verifymessage(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "verifymessage \"address\" \"signature\" \"message\"\n"
            "\nVerify a signed message.\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The Badcoin address used to create the signature.\n"
            "2. \"signature\"   (string, required) The base64-encoded signature (see signmessage).\n"
            "3. \"message\"     (string, required) The original message that was signed.\n"
            "\nResult:\n"
            "true|false        (boolean) Whether the signature is valid for the given address and message.\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds:\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature:\n"
            + HelpExampleCli("signmessage", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\" \"my message\"") +
            "\nVerify the signature:\n"
            + HelpExampleCli("verifymessage", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call:\n"
            + HelpExampleRpc("verifymessage", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\", \"signature\", \"my message\"")
        );

    LOCK(cs_main);

    std::string strAddress = request.params[0].get_str();
    std::string strSign = request.params[1].get_str();
    std::string strMessage = request.params[2].get_str();

    CTxDestination destination = DecodeDestination(strAddress);
    if (!IsValidDestination(destination)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");
    }

    const CKeyID* keyID = boost::get<CKeyID>(&destination);
    if (!keyID) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    }

    bool fInvalid = false;
    std::vector<unsigned char> vchSig = DecodeBase64(strSign.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey;
    if (!pubkey.RecoverCompact(ss.GetHash(), vchSig))
        return false;

    return (pubkey.GetID() == *keyID);
}

UniValue signmessagewithprivkey(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "signmessagewithprivkey \"privkey\" \"message\"\n"
            "\nSign a message with a private key.\n"
            "\nArguments:\n"
            "1. \"privkey\"     (string, required) The private key (WIF format) used to sign the message.\n"
            "2. \"message\"     (string, required) The message to sign.\n"
            "\nResult:\n"
            "\"signature\"      (string) Base64-encoded signature of the message.\n"
            "\nExamples:\n"
            "\nCreate a signature:\n"
            + HelpExampleCli("signmessagewithprivkey", "\"privkey\" \"my message\"") +
            "\nVerify the signature:\n"
            + HelpExampleCli("verifymessage", "\"MJPyMHUJiQpwM5ZkzKXDo5UPYTNh5yJ9Vf\" \"signature\" \"my message\"") +
            "\nAs a JSON-RPC call:\n"
            + HelpExampleRpc("signmessagewithprivkey", "\"privkey\", \"my message\"")
        );

    std::string strPrivkey = request.params[0].get_str();
    std::string strMessage = request.params[1].get_str();

    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strPrivkey);
    if (!fGood)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
    CKey key = vchSecret.GetKey();
    if (!key.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(vchSig.data(), vchSig.size());
}

UniValue setmocktime(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "setmocktime timestamp\n"
            "\nSet the node's mock time to the given timestamp (-regtest only).\n"
            "\nThis affects time-based logic such as block timestamps and mempool expiry\n"
            "in regression testing environments.\n"
            "\nArguments:\n"
            "1. timestamp  (integer, required) Unix time (seconds since epoch). Pass 0 to\n"
            "              return to using the system time.\n"
        );

    if (!Params().MineBlocksOnDemand())
        throw std::runtime_error("setmocktime is only available in regression testing (-regtest mode).");

    // For now, don't change mocktime if we're in the middle of validation, as
    // this could have an effect on mempool time-based eviction, as well as
    // IsCurrentForFeeEstimation() and IsInitialBlockDownload().
    // TODO: figure out the right way to synchronize around mocktime, and
    // ensure all call sites of GetTime() are accessing this safely.
    LOCK(cs_main);

    RPCTypeCheck(request.params, {UniValue::VNUM});
    SetMockTime(request.params[0].get_int64());

    return NullUniValue;
}

static UniValue RPCLockedMemoryInfo()
{
    LockedPool::Stats stats = LockedPoolManager::Instance().stats();
    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("used", uint64_t(stats.used)));
    obj.push_back(Pair("free", uint64_t(stats.free)));
    obj.push_back(Pair("total", uint64_t(stats.total)));
    obj.push_back(Pair("locked", uint64_t(stats.locked)));
    obj.push_back(Pair("chunks_used", uint64_t(stats.chunks_used)));
    obj.push_back(Pair("chunks_free", uint64_t(stats.chunks_free)));
    return obj;
}

#ifdef HAVE_MALLOC_INFO
static std::string RPCMallocInfo()
{
    char* ptr = nullptr;
    size_t size = 0;
    FILE* f = open_memstream(&ptr, &size);
    if (f) {
        malloc_info(0, f);
        fclose(f);
        if (ptr) {
            std::string rv(ptr, size);
            free(ptr);
            return rv;
        }
    }
    return "";
}
#endif

UniValue getmemoryinfo(const JSONRPCRequest& request)
{
    /* Please avoid using the word "pool" in this RPC interface or help,
     * as users may confuse it with the transaction memory pool.
     */
    if (request.fHelp || request.params.size() > 1)
        throw std::runtime_error(
            "getmemoryinfo (\"mode\")\n"
            "\nReturn information about memory usage in the Badcoin daemon.\n"
            "\nArguments:\n"
            "1. \"mode\"   (string, optional, default=\"stats\")\n"
            "   - \"stats\"      : General statistics about locked memory usage.\n"
            "   - \"mallocinfo\" : Low-level heap state as an XML string (glibc 2.10+ only).\n"
            "\nResult (mode=\"stats\"):\n"
            "{\n"
            "  \"locked\": {               (object) Information about the locked memory manager\n"
            "    \"used\": xxxxx,          (numeric) Bytes used\n"
            "    \"free\": xxxxx,          (numeric) Bytes available in current arenas\n"
            "    \"total\": xxxxxxx,       (numeric) Total bytes managed\n"
            "    \"locked\": xxxxxx,       (numeric) Bytes successfully locked. If less than total,\n"
            "                              some key material could be swapped to disk.\n"
            "    \"chunks_used\": xxxxx,   (numeric) Number of allocated chunks\n"
            "    \"chunks_free\": xxxxx    (numeric) Number of unused chunks\n"
            "  }\n"
            "}\n"
            "\nResult (mode=\"mallocinfo\"):\n"
            "\"<malloc version=\\\"1\\\">...\"\n"
            "\nExamples:\n"
            + HelpExampleCli("getmemoryinfo", "")
            + HelpExampleRpc("getmemoryinfo", "")
        );

    std::string mode = request.params[0].isNull() ? "stats" : request.params[0].get_str();
    if (mode == "stats") {
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("locked", RPCLockedMemoryInfo()));
        return obj;
    } else if (mode == "mallocinfo") {
#ifdef HAVE_MALLOC_INFO
        return RPCMallocInfo();
#else
        throw JSONRPCError(RPC_INVALID_PARAMETER, "mallocinfo is only available when compiled with glibc 2.10+");
#endif
    } else {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown mode " + mode);
    }
}

uint32_t getCategoryMask(UniValue cats)
{
    cats = cats.get_array();
    uint32_t mask = 0;
    for (unsigned int i = 0; i < cats.size(); ++i) {
        uint32_t flag = 0;
        std::string cat = cats[i].get_str();
        if (!GetLogCategory(&flag, &cat)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "unknown logging category " + cat);
        }
        if (flag == BCLog::NONE) {
            return 0;
        }
        mask |= flag;
    }
    return mask;
}

UniValue logging(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "logging ( <include> <exclude> )\n"
            "\nGet or modify the logging configuration.\n"
            "\nWithout arguments, returns the list of logging categories and whether they're active.\n"
            "With arguments, categories in \"include\" are enabled and categories in \"exclude\" are\n"
            "disabled (in that order). If a category appears in both, it is excluded.\n"
            "\nValid logging categories are: " +
            ListLogCategories() +
            "\nIn addition, the following special category names are available:\n"
            "  - \"all\",  \"1\" : all logging categories\n"
            "  - \"none\", \"0\" : disable all categories, even if others are specified\n"
            "\nArguments:\n"
            "1. \"include\"  (array of strings, optional) Categories to enable\n"
            "   [\n"
            "     \"category\"   (string) A valid logging category\n"
            "     ,...\n"
            "   ]\n"
            "2. \"exclude\"  (array of strings, optional) Categories to disable\n"
            "   [\n"
            "     \"category\"   (string) A valid logging category\n"
            "     ,...\n"
            "   ]\n"
            "\nResult:\n"
            "{\n"
            "  \"category\" : 0|1,   (numeric) 0 if inactive, 1 if active\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("logging", "\"[\\\"all\\\"]\" \"[\\\"http\\\"]\"")
            + HelpExampleRpc("logging", "[\"all\"], \"[\\\"libevent\\\"]\"")
        );
    }

    uint32_t originalLogCategories = logCategories;
    if (request.params[0].isArray()) {
        logCategories |= getCategoryMask(request.params[0]);
    }

    if (request.params[1].isArray()) {
        logCategories &= ~getCategoryMask(request.params[1]);
    }

    // Update libevent logging if BCLog::LIBEVENT has changed.
    // If the library version doesn't allow it, UpdateHTTPServerLogging() returns false,
    // in which case we should clear the BCLog::LIBEVENT flag.
    // Throw an error if the user has explicitly asked to change only the libevent
    // flag and it failed.
    uint32_t changedLogCategories = originalLogCategories ^ logCategories;
    if (changedLogCategories & BCLog::LIBEVENT) {
        if (!UpdateHTTPServerLogging(logCategories & BCLog::LIBEVENT)) {
            logCategories &= ~BCLog::LIBEVENT;
            if (changedLogCategories == BCLog::LIBEVENT) {
                throw JSONRPCError(
                    RPC_INVALID_PARAMETER,
                    "libevent logging cannot be updated when using libevent before v2.1.1.");
            }
        }
    }

    UniValue result(UniValue::VOBJ);
    std::vector<CLogCategoryActive> vLogCatActive = ListActiveLogCategories();
    for (const auto& logCatActive : vLogCatActive) {
        result.pushKV(logCatActive.category, logCatActive.active);
    }

    return result;
}

UniValue echo(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw std::runtime_error(
            "echo|echojson \"message\" ...\n"
            "\nSimply echo back the input arguments. This command is for testing.\n"
            "\nThe difference between echo and echojson is that echojson has argument conversion\n"
            "enabled in the client-side table in badcoin-cli and the GUI. There is no server-side\n"
            "difference between the two commands.\n"
        );

    return request.params;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "control",            "getmemoryinfo",          &getmemoryinfo,          {"mode"} },
    { "control",            "logging",                &logging,                {"include", "exclude"}},

    { "util",               "validateaddress",        &validateaddress,        {"address"} }, /* uses wallet if enabled */
    { "util",               "createmultisig",         &createmultisig,         {"nrequired","keys"} },
    { "util",               "verifymessage",          &verifymessage,          {"address","signature","message"} },
    { "util",               "signmessagewithprivkey", &signmessagewithprivkey, {"privkey","message"} },

    /* Not shown in help */
    { "hidden",             "setmocktime",            &setmocktime,            {"timestamp"} },
    { "hidden",             "echo",                   &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"} },
    { "hidden",             "echojson",               &echo,                   {"arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"} },
};

void RegisterMiscRPCCommands(CRPCTable& t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

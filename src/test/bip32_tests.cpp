// Copyright (c) 2013-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <base58.h>
#include <key.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <test/test_bitcoin.h>

#include <string>
#include <vector>

struct TestDerivation {
    std::string pub;
    std::string prv;
    unsigned int nChild;
};

struct TestVector {
    std::string strHexMaster;
    std::vector<TestDerivation> vDerive;

    explicit TestVector(std::string strHexMasterIn) : strHexMaster(strHexMasterIn) {}

    TestVector& operator()(std::string pub, std::string prv, unsigned int nChild) {
        vDerive.push_back(TestDerivation());
        TestDerivation &der = vDerive.back();
        der.pub = pub;
        der.prv = prv;
        der.nChild = nChild;
        return *this;
    }
};

TestVector test1 =
  TestVector("000102030405060708090a0b0c0d0e0f")
    ("2SLWCotUg6B2t1uQz1fRbXdSHxtkFT3jnCnsnknhBC8wu1kDq2sc4G84JWgy2C6iVezXzvzf1Ek2BpLKM5MKJbCkyN5uNiV7Qg6LjXBrrn8eu9yq",
     "2SLWCxCBMq5YEKuYMgeNvKFZt1ATdU3RDxfLqxsNMgGaasgVEzZXgLyZe9PGQWt8bkQqSqfDCFj25DXxk7bcpTGvAaHg9ykEZP4aNMnzLp97ffn3",
     0x80000000)
    ("2SLWCovk65tLM7AXyAGZJjPJvqvrMmsJ8asMoSQ85AEL1ywR7Q9uEgMbPdqKRyNHCQJSXwCwa6yZcYyPS3kHSYEVMRyN6v7ufbQEx37zR5oBFRii",
     "2SLWCxESmpnqhRAfLqFWdX1SWtCZjnryaLjpreUoFeMxhqsgXMqprmD6jGXcpJ9UwA8DQYi6td2LwbtocqMeY6sCMNMkg3cuhd2HG4odCFLLRKgN",
     1)
    ("2SLWCoxvDHftEVsR2zibkTeCoSfk1qFHEau66NinukW6pvaDKdhQNQA1hbQDfPSjmpF9stvZctQ1ifZsue42G1ZfF7dC3DKkX53FDfrXCNkeMU91",
     "2SLWCxGcu2aPaosYQfhZ5FGLPUwTPrExgLmZ9aoU6EdjWnWUjbPKzV1X3E6X3iCfzm3zSznraRqwNgbm2yJAmECzL9XF7ZPsRgn47jbwZnGbAjNh",
     0x80000002)
    ("2SLWCp1XVLCi6CkGSrtPqiEcAofnA4Q9krSg5iG2azqPz6Sam3YbVCur8VqmawTvcG7bCFSfMVmzL3WSt6PAYifCnB5LsoDif9zJuPYSLTzd2qur",
     "2SLWCxKEB57DSWkPpXsMAVrjkqwVY5PqCcK98vLhmUy2fxNrB1EX7HmMU8Y4yGEtcjHVwWFsy2JtJ71awCVuWpDuXGcgmv1RyqiJ4LsgnQp8r3iX",
     2)
    ("2SLWCp3ktAdq3PDwRwMsZQVpFhAwU9DSVV2zH9jutFnvTQt9NQeDrGgYVhREWcnuzfujviknXdnUH6FLzeoWYoxdytfp8i1GqzBCjQEMBcY4wheF",
     "2SLWCxMTZuYLPhE4ocLptC7wqjSerAD7wEuTLMpb4jvZ9GpQnNL9UMY3qL7XtwZJ5BwuNa41feE7s1q36fgoy1eSsU1GpKFX5Po7mVr9wvKrcpnU",
     1000000000)
    ("2SLWCp5UeeKSHWMKdThf1X3NyzswTwT1oJXoJwzJSW5JbcxXs7mdEo6o9xXPaFcpwXApeCT5598Q5dT3bFDuZup8pjvyFaYgvnenNDBy6rfxFxU8",
     "2SLWCxPBLPDwdpMT18gcLJfWa39eqxShF4QGNA4yczCwHUtoH5TYrsxJVbDgxaR5H6s2TBTQvRb6orsaDfrpLYiTbaxvMekHJWhsUnBiJfTNzqVz",
     0);

TestVector test2 =
  TestVector("fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a29f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542")
    ("2SLWCotUg6B2t1uQz1GvvdnWaR6U1hEtCtR8cSNUuV5giz8LhKrdFSGqEwb8cYCoUNEtxqYto9QgMDQpbo93XiPg8VzuKaRJsWaqPqZXy47WPqBE",
     "2SLWCxCBMq5YEKuYMgFtFRQeATNBPiEZeeHbfeTA5yDKQr4c7HYYsX8LaaHRzrwuevvPY5hxd11UXVdzwD8LvVhJsBezvSAJoXQV6w3caB5wyMLT",
     0)
    ("2SLWCowkRMtiqVvF4XYXDmZtTUtsY4CdTbPctzYEGetiB4YCf1q5E1YBK6Rhs7fmNAKuQ3sNy1dg6cVfZxJUDbYNYqbZbrU3YQvymy3edVFo5ecs",
     "2SLWCxFT76oEBovNSCXUYZC23XAav5CJuMG5xCcuT92LrvUU4yWzr6Pgej81FSTBTZzENVNCSu1tFvoR4Zdyk3yov7tAKSLiruter4TX8YQsbrvK",
     0xFFFFFFFF)
    ("2SLWCoxuUcVkMspRAhQjQC1t6xEZ6SwrN8snb5hJoaHcwiGy3U8JgdRbdJVVTj41Uv4WVqPJWfUXhui6AcyLZNSrfZk2EPYfbmfEnqRvEL6yyVbM",
     "2SLWCxGcAMQFiBpYYNPgiye1gzWGUTwXotkFeHmyz4RFdaDETRpEJiH6xwBnr3ofSJExrPbXLg35tyovAzsiWnMR3hBbss4xCgQP6rErfJnM6ik5",
     1)
    ("2SLWCp1iT2WjJ3TiMo2QYFg2fvBVKnUisF4hrQ8ofRMHH2AKV8xAqvw8LCPsqZrCchwCnRZm4W7cwYqPzhmbaHMHcq5V8ypsSzCfzmCbBaVKzNMr",
     "2SLWCxKR8mREeMTqjU1Ms3JAFxTChoUQJzwAucDUquUuxt6au6e6U1ndfq6BDtbs4KxAYhYt3RLAHPMbPKaVfqEwRhfRAzqV1XWMVL3HPnYiE3WE",
     0xFFFFFFFE)
    ("2SLWCp2tUwUfeerg55ye8z9rDHhBWhDCEJ3LMRqXYx4ygrvajbJVn2c4LivEwCWL7uJjEP1QhJcgebSirMQkxyEQ8YSBbFjBwYVmQdRfm7TeBuFW",
     "2SLWCxLbAgPAzxroSkxbTmmyoKxttiCsg3uoQdvCjSCcNirr9YzRQ7TZgMcYKXJbSxNYqF8Ajj9wGmjAy8hb3MBqvPW6Lpn2KfiP7fdHurH7EBm3",
     2)
    ("2SLWCp4FWtutAB2zJqEq2iDyuGSZV6GjLYNFS619SMrLBViSKfVqMCEz6uVBwGuQQ2t5vu4cvaYC4V9jqqqpToL62rh3Atu22Us49Jirwn7W6oPX",
     "2SLWCxMxCdpPWV37gWDnMVr7VJiGs7GQnJEiVJ5pcqyxsMehjdBkyH6VSYBVKbiGT7PduHS2U1ua6Dq1fBFUZ5WD9SUq4ddXpj8WqMzdTvqdcWpV",
     0);

TestVector test3 =
  TestVector("4b381541583be4423346c643850da4b320e46a87ae3d2a4e6da11eba819cd4acba45d239319ac14f863b8d5ab5a0d0c64d2e8a1e7d1457df2e5a3c51c73235be")
    ("2SLWCotUg6B2t1uQyzLP6AB1Nd5j3ZtDJeHHt9iFTtNiHFpRVJ6rX6R5CtrN92CLTBqUc1deeyQZMc5eTESgSZ6iZvLZThfD6F9HEdH3faYMGHWp",
     "2SLWCxCBMq5YEKuYMfKLQwo8xfMSRastkQ9kwMnveNWLy7kguFnn9BGaYXYfXLwdpswcsAQdNZwAdQ4jeZmxEj8THy8WAxSsSiqq5DmGvq2AgAZJ",
      0x80000000)
    ("2SLWCovqsq8s1hGnLBsUVPKLsgLUiGEpgo8hnZCVQhcQ8C8LrZ7yyfur4yK9rKvAxvC5MD19VkNK6T2qwrucnVLekVaqQNm9ynzn37TQaEvpaF8S",
     "2SLWCxEYZa3NN1GuhrrRpAwUTicC6HEW8Z1AqmHAbBk2p44cGWoubkmMQc1TEehzBDtwncZGnasxW3oN7DKbPJM33ithoPanVU16tLnEanQc53Hg",
      0);

void RunTest(const TestVector &test) {
    std::vector<unsigned char> seed = ParseHex(test.strHexMaster);
    CExtKey key;
    CExtPubKey pubkey;
    key.SetMaster(seed.data(), seed.size());
    pubkey = key.Neuter();
    for (const TestDerivation &derive : test.vDerive) {
        unsigned char data[74];
        key.Encode(data);
        pubkey.Encode(data);

        // Test private key
        CBitcoinExtKey b58key; b58key.SetKey(key);
        BOOST_CHECK(b58key.ToString() == derive.prv);

        CBitcoinExtKey b58keyDecodeCheck(derive.prv);
        CExtKey checkKey = b58keyDecodeCheck.GetKey();
        assert(checkKey == key); //ensure a base58 decoded key also matches

        // Test public key
        CBitcoinExtPubKey b58pubkey; b58pubkey.SetKey(pubkey);
        BOOST_CHECK(b58pubkey.ToString() == derive.pub);

        CBitcoinExtPubKey b58PubkeyDecodeCheck(derive.pub);
        CExtPubKey checkPubKey = b58PubkeyDecodeCheck.GetKey();
        assert(checkPubKey == pubkey); //ensure a base58 decoded pubkey also matches

        // Derive new keys
        CExtKey keyNew;
        BOOST_CHECK(key.Derive(keyNew, derive.nChild));
        CExtPubKey pubkeyNew = keyNew.Neuter();
        if (!(derive.nChild & 0x80000000)) {
            // Compare with public derivation
            CExtPubKey pubkeyNew2;
            BOOST_CHECK(pubkey.Derive(pubkeyNew2, derive.nChild));
            BOOST_CHECK(pubkeyNew == pubkeyNew2);
        }
        key = keyNew;
        pubkey = pubkeyNew;

        CDataStream ssPub(SER_DISK, CLIENT_VERSION);
        ssPub << pubkeyNew;
        BOOST_CHECK(ssPub.size() == 75);

        CDataStream ssPriv(SER_DISK, CLIENT_VERSION);
        ssPriv << keyNew;
        BOOST_CHECK(ssPriv.size() == 75);

        CExtPubKey pubCheck;
        CExtKey privCheck;
        ssPub >> pubCheck;
        ssPriv >> privCheck;

        BOOST_CHECK(pubCheck == pubkeyNew);
        BOOST_CHECK(privCheck == keyNew);
    }
}

BOOST_FIXTURE_TEST_SUITE(bip32_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bip32_test1) {
    RunTest(test1);
}

BOOST_AUTO_TEST_CASE(bip32_test2) {
    RunTest(test2);
}

BOOST_AUTO_TEST_CASE(bip32_test3) {
    RunTest(test3);
}

BOOST_AUTO_TEST_SUITE_END()

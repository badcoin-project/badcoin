// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <miner.h>
#include <pow.h>
#include <random.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <validationinterface.h>

#include <algorithm>
#include <memory>
#include <string>

#include <boost/thread.hpp>

struct RegtestingSetup : public TestingSetup {
    RegtestingSetup() : TestingSetup(CBaseChainParams::REGTEST) {}
};

BOOST_FIXTURE_TEST_SUITE(validation_block_tests, RegtestingSetup)

struct TestSubscriber : public CValidationInterface {
    uint256 m_expected_tip;

    TestSubscriber(uint256 tip) : m_expected_tip(tip) {}

    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload)
    {
        BOOST_CHECK_EQUAL(m_expected_tip, pindexNew->GetBlockHash());
    }

    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex, const std::vector<CTransactionRef>& txnConflicted)
    {
        BOOST_CHECK_EQUAL(m_expected_tip, block->hashPrevBlock);
        BOOST_CHECK_EQUAL(m_expected_tip, pindex->pprev->GetBlockHash());

        m_expected_tip = block->GetHash();
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block)
    {
        BOOST_CHECK_EQUAL(m_expected_tip, block->GetHash());

        m_expected_tip = block->hashPrevBlock;
    }
};

std::shared_ptr<CBlock> Block(const CBlockIndex* pindexPrev)
{
    static int i = 0;

    CScript pubKey;
    pubKey << i++ << OP_TRUE;

    auto ptemplate = BlockAssembler(Params()).CreateNewBlock(pubKey, ALGO_SHA256D);
    auto pblock = std::make_shared<CBlock>(ptemplate->block);
    pblock->hashPrevBlock = pindexPrev->GetBlockHash();
    pblock->nTime = std::max<int64_t>(
        pindexPrev->GetBlockTime() + Params().GetConsensus().nPowTargetSpacing * 3,
        pindexPrev->GetMedianTimePast() + 1);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock.get(), pblock->GetAlgo(), Params().GetConsensus());

    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vout.resize(1);
    txCoinbase.vin[0].scriptSig = CScript() << pindexPrev->nHeight + 1 << OP_0;
    txCoinbase.vin[0].scriptWitness.SetNull();
    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));

    return pblock;
}

std::shared_ptr<CBlock> FinalizeBlock(std::shared_ptr<CBlock> pblock)
{
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);

    while (!CheckProofOfWork(pblock->GetPoWHash(pblock->GetAlgo(), Params().GetConsensus()), pblock->GetAlgo(), pblock->nBits, Params().GetConsensus())) {
        ++(pblock->nNonce);
    }

    return pblock;
}

CBlockIndex* MakeBlockIndex(const CBlock& block, const CBlockIndex* pindexPrev, std::vector<std::unique_ptr<CBlockIndex>>& block_indices, std::vector<std::unique_ptr<uint256>>& block_hashes)
{
    block_hashes.emplace_back(new uint256(block.GetHash()));
    block_indices.emplace_back(new CBlockIndex(block));
    CBlockIndex* pindex = block_indices.back().get();
    pindex->phashBlock = block_hashes.back().get();
    pindex->pprev = const_cast<CBlockIndex*>(pindexPrev);
    pindex->nHeight = pindexPrev->nHeight + 1;
    pindex->BuildSkip();
    return pindex;
}

// construct a valid block
const std::shared_ptr<const CBlock> GoodBlock(const CBlockIndex* pindexPrev, std::vector<std::unique_ptr<CBlockIndex>>& block_indices, std::vector<std::unique_ptr<uint256>>& block_hashes, CBlockIndex*& pindexOut)
{
    auto block = FinalizeBlock(Block(pindexPrev));
    pindexOut = MakeBlockIndex(*block, pindexPrev, block_indices, block_hashes);
    return block;
}

// construct an invalid block (but with a valid header)
const std::shared_ptr<const CBlock> BadBlock(const CBlockIndex* pindexPrev, std::vector<std::unique_ptr<CBlockIndex>>& block_indices, std::vector<std::unique_ptr<uint256>>& block_hashes)
{
    auto pblock = Block(pindexPrev);

    CMutableTransaction coinbase_spend;
    coinbase_spend.vin.push_back(CTxIn(COutPoint(pblock->vtx[0]->GetHash(), 0), CScript(), 0));
    coinbase_spend.vout.push_back(pblock->vtx[0]->vout[0]);

    CTransactionRef tx = MakeTransactionRef(coinbase_spend);
    pblock->vtx.push_back(tx);

    auto ret = FinalizeBlock(pblock);
    MakeBlockIndex(*ret, pindexPrev, block_indices, block_hashes);
    return ret;
}

void BuildChain(const CBlockIndex* pindexPrev, int height, const unsigned int invalid_rate, const unsigned int branch_rate, const unsigned int max_size, std::vector<std::shared_ptr<const CBlock>>& blocks, std::vector<std::unique_ptr<CBlockIndex>>& block_indices, std::vector<std::unique_ptr<uint256>>& block_hashes)
{
    if (height <= 0 || blocks.size() >= max_size) return;

    bool gen_invalid = GetRand(100) < invalid_rate;
    bool gen_fork = GetRand(100) < branch_rate;

    CBlockIndex* pindexNew = nullptr;
    const std::shared_ptr<const CBlock> pblock = gen_invalid ? BadBlock(pindexPrev, block_indices, block_hashes) : GoodBlock(pindexPrev, block_indices, block_hashes, pindexNew);
    blocks.push_back(pblock);
    if (!gen_invalid) {
        BuildChain(pindexNew, height - 1, invalid_rate, branch_rate, max_size, blocks, block_indices, block_hashes);
    }

    if (gen_fork) {
        blocks.push_back(GoodBlock(pindexPrev, block_indices, block_hashes, pindexNew));
        BuildChain(pindexNew, height - 1, invalid_rate, branch_rate, max_size, blocks, block_indices, block_hashes);
    }
}

BOOST_AUTO_TEST_CASE(processnewblock_signals_ordering)
{
    // build a large-ish chain that's likely to have some forks
    std::vector<std::shared_ptr<const CBlock>> blocks;
    std::vector<std::unique_ptr<CBlockIndex>> block_indices;
    std::vector<std::unique_ptr<uint256>> block_hashes;
    while (blocks.size() < 50) {
        blocks.clear();
        block_indices.clear();
        block_hashes.clear();
        const CBlockIndex* pindexRoot = nullptr;
        {
            LOCK(cs_main);
            pindexRoot = chainActive.Tip();
        }
        BuildChain(pindexRoot, 100, 15, 10, 500, blocks, block_indices, block_hashes);
    }

    bool ignored;
    CValidationState state;
    std::vector<CBlockHeader> headers;
    std::transform(blocks.begin(), blocks.end(), std::back_inserter(headers), [](std::shared_ptr<const CBlock> b) { return b->GetBlockHeader(); });

    // Process all the headers so we understand the toplogy of the chain
    BOOST_CHECK(ProcessNewBlockHeaders(headers, state, Params()));

    // Connect the genesis block and drain any outstanding events
    ProcessNewBlock(Params(), std::make_shared<CBlock>(Params().GenesisBlock()), true, &ignored);
    SyncWithValidationInterfaceQueue();

    // subscribe to events (this subscriber will validate event ordering)
    const CBlockIndex* initial_tip = nullptr;
    {
        LOCK(cs_main);
        initial_tip = chainActive.Tip();
    }
    TestSubscriber sub(initial_tip->GetBlockHash());
    RegisterValidationInterface(&sub);

    // create a bunch of threads that repeatedly process a block generated above at random
    // this will create parallelism and randomness inside validation - the ValidationInterface
    // will subscribe to events generated during block validation and assert on ordering invariance
    boost::thread_group threads;
    boost::mutex failures_mutex;
    std::vector<std::string> failures;
    for (int i = 0; i < 10; i++) {
        threads.create_thread([&blocks, &failures, &failures_mutex]() {
            bool ignored;
            for (int i = 0; i < 1000; i++) {
                auto block = blocks[GetRand(blocks.size() - 1)];
                ProcessNewBlock(Params(), block, true, &ignored);
            }

            // to make sure that eventually we process the full chain - do it here
            for (auto block : blocks) {
                if (block->vtx.size() == 1) {
                    bool processed = ProcessNewBlock(Params(), block, true, &ignored);
                    if (!processed) {
                        boost::mutex::scoped_lock lock(failures_mutex);
                        failures.push_back(block->GetHash().ToString());
                    }
                }
            }
        });
    }

    threads.join_all();
    while (GetMainSignals().CallbacksPending() > 0) {
        MilliSleep(100);
    }

    UnregisterValidationInterface(&sub);

    if (!failures.empty()) {
        BOOST_ERROR("valid block was not processed: " + failures.front());
    }
    BOOST_CHECK_EQUAL(sub.m_expected_tip, chainActive.Tip()->GetBlockHash());
}

BOOST_AUTO_TEST_SUITE_END()

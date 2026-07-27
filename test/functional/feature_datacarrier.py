#!/usr/bin/env python3
# Copyright (c) 2026 The Badcoin Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test OP_RETURN / data-carrier relay policy boundaries.

Default MAX_OP_RETURN_RELAY is 516 script bytes, which allows a single
canonical 512-byte metadata payload. This is mempool/relay/mining
standardness only — not consensus.
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes_bi,
    sync_blocks,
)


def expected_nulldata_script_size(payload_len):
    """Match CScript << vector push encoding overhead."""
    if payload_len < 0x4c:
        return 1 + 1 + payload_len  # OP_RETURN + direct length + payload
    if payload_len <= 0xff:
        return 1 + 1 + 1 + payload_len  # + OP_PUSHDATA1
    return 1 + 1 + 2 + payload_len  # + OP_PUSHDATA2 + 2-byte length


class DataCarrierTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True
        # Node 0: new default (516), require standard
        # Node 1: legacy 83 ceiling, require standard
        # Node 2: permissive (accept nonstandard) — mines a policy-nonstandard
        #         tx into a valid block for the consensus check
        self.extra_args = [
            ["-acceptnonstdtxn=0"],
            ["-acceptnonstdtxn=0", "-datacarriersize=83"],
            ["-acceptnonstdtxn=1"],
        ]

    def create_funded_data_tx(self, node, payload_len):
        raw = node.createrawtransaction([], {"data": "11" * payload_len})
        funded = node.fundrawtransaction(raw)
        signed = node.signrawtransaction(funded["hex"])
        assert_equal(signed["complete"], True)
        decoded = node.decoderawtransaction(signed["hex"])
        data_vouts = [v for v in decoded["vout"] if v["scriptPubKey"]["type"] == "nulldata"]
        assert_equal(len(data_vouts), 1)
        script_hex = data_vouts[0]["scriptPubKey"]["hex"]
        assert_equal(len(script_hex) // 2, expected_nulldata_script_size(payload_len))
        return signed["hex"]

    def run_test(self):
        self.nodes[0].generate(110)
        sync_blocks(self.nodes)

        for n in (1, 2):
            self.nodes[0].sendtoaddress(self.nodes[n].getnewaddress(), 25)
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        self.log.info("New default accepts a 512-byte payload (516-byte script)")
        tx_512 = self.create_funded_data_tx(self.nodes[0], 512)
        txid_512 = self.nodes[0].sendrawtransaction(tx_512)
        assert txid_512 in self.nodes[0].getrawmempool()

        self.log.info("513-byte payload is rejected under the new default")
        tx_513 = self.create_funded_data_tx(self.nodes[0], 513)
        assert_raises_rpc_error(-26, "scriptpubkey", self.nodes[0].sendrawtransaction, tx_513)

        self.log.info("Legacy -datacarriersize=83 rejects a 512-byte payload")
        tx_512_legacy = self.create_funded_data_tx(self.nodes[1], 512)
        assert_raises_rpc_error(-26, "scriptpubkey", self.nodes[1].sendrawtransaction, tx_512_legacy)

        self.log.info("Legacy -datacarriersize=83 still accepts an 80-byte payload")
        tx_80 = self.create_funded_data_tx(self.nodes[1], 80)
        txid_80 = self.nodes[1].sendrawtransaction(tx_80)
        assert txid_80 in self.nodes[1].getrawmempool()

        self.log.info("-datacarrier=0 rejects otherwise-valid data carriers")
        self.stop_node(1)
        self.start_node(1, extra_args=["-acceptnonstdtxn=0", "-datacarrier=0"])
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes)
        if self.nodes[1].getbalance() < 1:
            self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 10)
            self.nodes[0].generate(1)
            sync_blocks(self.nodes)
        tx_disabled = self.create_funded_data_tx(self.nodes[1], 80)
        assert_raises_rpc_error(-26, "scriptpubkey", self.nodes[1].sendrawtransaction, tx_disabled)

        # Restore legacy-83 node for the consensus check below
        self.stop_node(1)
        self.start_node(1, extra_args=["-acceptnonstdtxn=0", "-datacarriersize=83"])
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes)

        self.log.info("Restrictive node rejects 512-byte tx from mempool but accepts it in a block")
        if self.nodes[2].getbalance() < 1:
            self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 10)
            self.nodes[0].generate(1)
            sync_blocks(self.nodes)
        tx_512_perm = self.create_funded_data_tx(self.nodes[2], 512)
        txid_perm = self.nodes[2].sendrawtransaction(tx_512_perm)
        assert txid_perm in self.nodes[2].getrawmempool()
        assert_raises_rpc_error(-26, "scriptpubkey", self.nodes[1].sendrawtransaction, tx_512_perm)

        blockhash = self.nodes[2].generate(1)[0]
        sync_blocks(self.nodes)
        block = self.nodes[1].getblock(blockhash)
        assert txid_perm in block["tx"]
        assert_equal(self.nodes[1].getrawtransaction(txid_perm, True)["confirmations"], 1)

        self.log.info("Passed")


if __name__ == '__main__':
    DataCarrierTest().main()

#!/usr/bin/env python3
# Copyright (c) 2026 The Badcoin Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test P2P receive boundary for MAX_PROTOCOL_MESSAGE_LENGTH (8 MiB).

After the version handshake:
1. A claimed body size of exactly 8 MiB is accepted at the size gate
   (connection stays up while waiting for the body).
2. A claimed body size of 8 MiB + 1 is rejected and the peer is
   disconnected ("Oversized message").
3. Ordinary small P2P traffic still works under the new cap.
"""

import struct
import time

from test_framework.mininode import (
    MAGIC_BYTES,
    P2PInterface,
    mininode_lock,
    msg_ping,
    network_thread_start,
)
from test_framework.messages import sha256
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, wait_until

# Must match src/net.h
MAX_PROTOCOL_MESSAGE_LENGTH = 8 * 1024 * 1024


class MessageSizeP2P(P2PInterface):
    def send_claimed_size(self, command, claimed_size, body=b""):
        """Send a P2P frame with an explicit claimed payload length."""
        cmd = command.encode('ascii')
        assert len(cmd) <= 12
        tmsg = MAGIC_BYTES[self.network]
        tmsg += cmd + b"\x00" * (12 - len(cmd))
        tmsg += struct.pack("<I", claimed_size)
        tmsg += sha256(sha256(body))[:4]
        tmsg += body
        with mininode_lock:
            self.sendbuf += tmsg


class MessageSizeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        self.nodes[0].generate(1)

        # Documented size-budget regressions from the issue #1 measurement.
        assert 178723 < MAX_PROTOCOL_MESSAGE_LENGTH
        assert 2000 * 4096 < MAX_PROTOCOL_MESSAGE_LENGTH

        under = self.nodes[0].add_p2p_connection(MessageSizeP2P())
        over = self.nodes[0].add_p2p_connection(MessageSizeP2P())
        ok = self.nodes[0].add_p2p_connection(MessageSizeP2P())
        network_thread_start()

        self.log.info("Exact 8 MiB claimed size stays under the receive gate")
        under.wait_for_verack()
        under.send_claimed_size("ping", MAX_PROTOCOL_MESSAGE_LENGTH)
        time.sleep(1)
        assert under.connected

        self.log.info("8 MiB + 1 claimed size disconnects the peer")
        over.wait_for_verack()
        over.send_claimed_size("ping", MAX_PROTOCOL_MESSAGE_LENGTH + 1)
        wait_until(lambda: not over.connected, timeout=10)

        self.log.info("Ordinary small P2P traffic still works under 8 MiB")
        ok.wait_for_verack()
        ok.send_message(msg_ping())
        time.sleep(1)
        assert ok.connected
        assert_equal(ok.connected, True)


if __name__ == '__main__':
    MessageSizeTest().main()

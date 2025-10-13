#!/usr/bin/env python3
# Copyright (c) 2025 The Juno Moneta developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php.

"""
RandomX Epoch Transition Test

Test that mining works correctly across RandomX epoch boundaries.
Epoch boundaries occur every 1536 blocks with a 96-block lag.

Epoch 0: blocks 0-1631 (genesis seed: 0x08...)
Epoch 1: blocks 1632-3167 (uses block 1536 hash as seed)
Epoch 2: blocks 3168-4703 (uses block 3072 hash as seed)
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    start_nodes,
    connect_nodes_bi,
    sync_blocks,
)

class RandomXEpochsTest(BitcoinTestFramework):
    def setup_network(self, split=False):
        # Start 2 nodes for testing
        self.nodes = start_nodes(2, self.options.tmpdir)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        print("Testing RandomX epoch transitions...")

        # Start with no blocks mined (except genesis)
        assert_equal(self.nodes[0].getblockcount(), 0)

        # Mine blocks in epoch 0
        print("\nMining blocks in epoch 0...")
        self.nodes[0].generate(100)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 100)
        assert_equal(self.nodes[1].getblockcount(), 100)

        # Verify blocks are valid
        for i in range(1, 101):
            blockhash = self.nodes[0].getblockhash(i)
            block = self.nodes[0].getblock(blockhash)
            assert 'height' in block
            assert_equal(block['height'], i)

        print("✓ Epoch 0 mining successful")

        # Mine blocks approaching first epoch transition
        print("\nMining blocks approaching epoch boundary at 1631...")

        # Mine to block 1600
        blocks_to_mine = 1600 - 100
        self.nodes[0].generate(blocks_to_mine)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1600)

        print(f"✓ At block 1600, approaching transition")

        # Mine blocks 1601-1630 (approaching transition)
        print("\nMining blocks 1601-1630 (approaching last epoch 0 blocks)...")
        self.nodes[0].generate(30)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1630)

        # Get hash of block 1536 (will become seed for next epoch)
        block_1536_hash = self.nodes[0].getblockhash(1536)
        print(f"Block 1536 hash (future seed): {block_1536_hash}")

        print("✓ At block 1630, approaching transition")

        # Mine block 1631 (last block of epoch 0)
        print("\nMining block 1631 (last epoch 0 block)...")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1631)

        block_1631 = self.nodes[0].getblock(self.nodes[0].getblockhash(1631))
        assert_equal(block_1631['height'], 1631)
        print("✓ Block 1631 mined successfully (last epoch 0 block)")

        # Mine block 1632 (first block with new seed from block 1536)
        print("\nMining block 1632 (first epoch 1 block, uses block 1536 seed)...")
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1632)

        block_1632 = self.nodes[0].getblock(self.nodes[0].getblockhash(1632))
        assert_equal(block_1632['height'], 1632)
        print("✓ Block 1632 mined successfully (first epoch 1 block)")

        # Mine several more blocks in epoch 1 to ensure stability
        print("\nMining blocks 1633-1700 to verify epoch 1 stability...")
        self.nodes[0].generate(68)
        self.sync_all()
        assert_equal(self.nodes[0].getblockcount(), 1700)

        print("✓ Epoch 1 mining stable")

        # Verify all blocks are valid on both nodes
        print("\nVerifying blockchain consistency across nodes...")
        for height in [0, 100, 1000, 1536, 1600, 1631, 1632, 1700]:
            hash0 = self.nodes[0].getblockhash(height)
            hash1 = self.nodes[1].getblockhash(height)
            assert_equal(hash0, hash1)
            print(f"✓ Block {height}: {hash0[:16]}... matches on both nodes")

        # Test that both nodes can mine blocks in epoch 1
        print("\nTesting that node 1 can also mine in epoch 1...")
        initial_height = self.nodes[1].getblockcount()
        self.nodes[1].generate(5)
        self.sync_all()
        assert_equal(self.nodes[1].getblockcount(), initial_height + 5)
        assert_equal(self.nodes[0].getblockcount(), initial_height + 5)

        print("✓ Both nodes can mine successfully in epoch 1")

        # Summary
        final_height = self.nodes[0].getblockcount()
        print(f"\n{'='*60}")
        print(f"RandomX Epoch Transition Test: SUCCESS")
        print(f"{'='*60}")
        print(f"Final blockchain height: {final_height}")
        print(f"Epoch 0 blocks: 0-1631")
        print(f"Epoch 1 blocks: 1632-{final_height}")
        print(f"Block 1536 hash (epoch 1 seed): {block_1536_hash}")
        print(f"All blocks validated successfully")
        print(f"{'='*60}")

if __name__ == '__main__':
    RandomXEpochsTest().main()

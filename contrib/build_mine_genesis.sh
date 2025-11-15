#!/bin/bash
# Build genesis mining tool using the project's compiler setup

cd "$(dirname "$0")/.."

echo "Building genesis mining tool..."

# Use the same compiler as the main build
depends/x86_64-pc-linux-gnu/native/bin/clang++ \
    -target x86_64-pc-linux-gnu \
    -stdlib=libc++ \
    -m64 \
    -std=c++17 \
    -O3 \
    -I./src \
    -I./src/crypto/randomx \
    -I./src/rust/include \
    -I./src/rust/gen/include \
    -I./src/univalue/include \
    -I./src/secp256k1/include \
    -I./depends/x86_64-pc-linux-gnu/include \
    project_notes/mine_genesis.cpp \
    project_notes/genesis_stubs.cpp \
    src/rust/gen/src/*.cpp \
    src/rust/gen/src/sapling/*.cpp \
    -o contrib/mine_genesis \
    src/libbitcoin_server.a \
    src/libbitcoin_common.a \
    src/libbitcoin_util.a \
    src/libbitcoin_consensus.a \
    src/libbitcoin_script.a \
    src/crypto/libbitcoin_crypto.a \
    src/crypto/libbitcoin_crypto_sse41.a \
    src/crypto/libbitcoin_crypto_avx2.a \
    src/crypto/libbitcoin_crypto_shani.a \
    src/libzcash.a \
    src/libcxxbridge.a \
    target/x86_64-unknown-linux-gnu/release/librustzcash.a \
    src/leveldb/libleveldb.a \
    src/leveldb/libmemenv.a \
    src/crc32c/libcrc32c.a \
    src/crc32c/libcrc32c_sse42.a \
    src/univalue/.libs/libunivalue.a \
    src/secp256k1/.libs/libsecp256k1.a \
    -L./depends/x86_64-pc-linux-gnu/lib \
    -lboost_system \
    -lboost_filesystem \
    -lboost_thread \
    -lboost_chrono \
    -lboost_program_options \
    -lsodium \
    -levent \
    -levent_pthreads \
    -lpthread \
    -ldl \
    -lrt \
    -lc++ \
    -lc++abi \
    2>&1

if [ $? -eq 0 ]; then
    echo ""
    echo "Genesis mining tool built successfully: contrib/mine_genesis"
    echo ""
    echo "Usage:"
    echo "  ./contrib/mine_genesis main     # Mine mainnet genesis"
    echo "  ./contrib/mine_genesis test     # Mine testnet genesis"
    echo "  ./contrib/mine_genesis regtest  # Mine regtest genesis"
else
    echo "Build failed!"
    exit 1
fi

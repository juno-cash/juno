# Juno

Privacy-focused cryptocurrency with CPU mining (RandomX) and a 21M coin supply.

Forked from Zcash v6.10.0

## Features

- **RandomX mining** - CPU-optimized, ASIC-resistant (replaces Equihash)
- **21M coin supply** - Capped at block 9,991,569
- **100% to miners** - No developer tax or funding streams
- **60-second blocks** - Constant block time
- **Orchard privacy** - Mandatory shielded transactions after block 8
- **Fair launch** - No pre-mine, gradual ramp-up emission
- **1000 block maturity** - Enhanced security for CPU mining

## Quick Start

### Build

```bash
./zcutil/build.sh -j$(nproc)
```

### Mainnet

Create `~/.junocash/junocash.conf`:
```
rpcuser=user
rpcpassword=STRONG_PASSWORD
```

```bash
./src/junocashd -daemon
./src/junocash-cli getblockchaininfo
```

## Mining

### Enable Mining

In `junocash.conf`:
```
gen=1
```

Or via RPC:
```bash
./src/junocash-cli setgenerate true
./src/junocash-cli getmininginfo
```

### Spend Mined Coins

1. Wait 1000 blocks for maturity
2. Shield to Orchard:
```bash
./src/junocash-cli z_shieldcoinbase "*" YOUR_ORCHARD_ADDRESS
```
3. Send from Orchard (simple):
```bash
./src/junocash-cli z_send FROM_ORCHARD TO_ORCHARD 1.0
```

Or use z_sendmany for multiple recipients:
```bash
./src/junocash-cli z_sendmany FROM_ORCHARD '[{"address":"TO_ORCHARD","amount":1.0}]'
```

## Common Commands

```bash
# Get new account and address
./src/junocash-cli z_getnewaccount
./src/junocash-cli z_getaddressforaccount 0

# Check balance
./src/junocash-cli z_getbalanceforaccount 0

# Send to single recipient (simple)
./src/junocash-cli z_send "FROM_ADDRESS" "TO_ADDRESS" 1.0

# Send to multiple recipients
./src/junocash-cli z_sendmany "FROM_ADDRESS" '[{"address":"TO_ADDRESS","amount":1.0}]'

# List transactions
./src/junocash-cli listtransactions

# Stop daemon
./src/junocash-cli stop
```

---

## Network Parameters

### Networks

| Network | Magic Bytes | P2P Port | RPC Port | Address Prefix |
|---------|------------|----------|----------|----------------|
| Mainnet | 0xf9beb4d9 | 8234 | 8233 | jm |
| Testnet | 0x0b110907 | 18234 | 18233 | jmtest |
| Regtest | 0xfabfb5da | 18345 | 18344 | jmregtest |

### Consensus

| Parameter | Value |
|-----------|-------|
| Block Time | 60s |
| Coinbase Maturity | 1000 blocks |
| Max Supply | 21M JNO |
| PoW Algorithm | RandomX |
| Miner Reward | 100% |
| BIP-44 Coin Type | 8133 |

---

## Emission Schedule

### Maximum Supply
- **Total**: 21,000,000 coins
- **Supply exhausted**: Block 9,991,569
- **No subsidy after cap**

### Schedule Phases

**Phase 0: Genesis Buffer (0-9)**
- 0 JNO - Prevents pre-mine

**Phase 1: Ramp-Up (10-30,009)**
- Doubles every 5,000 blocks: 0.25 → 0.5 → 1 → 2 → 4 → 8 coins
- Duration: ~20.8 days

**Plateau (30,010-100,009)**
- 10 coins per block
- Duration: ~48.6 days

**Phase 2: First Year (100,010-625,609)**
- 8 JNO per block
- Duration: 365 days (525,600 blocks)
- Total emission: ~4.2M coins

**Phase 3: 4-Year Halvings (625,610-9,991,569)**
- Starts at 4 coins (pre-halved from 8)
- Halves every 2,103,840 blocks (4 years)
- Schedule: 4 → 2 → 1 → 0.5 → 0.25 → ...
- Duration: ~44.5 years to supply cap

### Transaction Rules (After Block 8)
- **Coinbase**: Transparent outputs only
- **Coinbase spends**: Must go to Orchard
- **All other transactions**: Orchard-to-Orchard only
- **Forbidden**: Sprout outputs, Sapling outputs (except migration), transparent outputs (except coinbase)

### Economics Changes
- **Removed**: All funding streams (ZIP207/ZIP214)
- **Removed**: Founders' reward, Canopy streams, NU6 lockbox
- **Result**: 100% of block subsidy + fees to miners

### Optional Developer Donation

You can throw some crumbs at the developers if you want

- **Default**: Off (0%)
- **Config (junocash.conf)**: `donationpercentage=0-100` and `donationaddress=<address>`
- **RPC**: `setdonationpercentage X` and `setdonationaddress <address>`
- **Behavior**: When enabled, splits coinbase into two outputs (miner + donation)
- **Address**: Default is `t1HwfuDqt2oAVexgpjDHg9yB7UpCKSmEES7` (mainnet)

### RPC Changes
- **Renamed**: Transparent-only commands prefixed with `t_` (t_getbalance, t_sendmany, etc.)
- **Enhanced getblocktemplate**: Includes RandomX seed information

### Build System
- **RandomX integration**: Custom Makefile rules for x86/ARM64/RISC-V assembly
- **Rust patches**: Custom zcash_protocol crate for address HRPs (depends/patches/zcash_protocol/)
- **Platform**: Multi-architecture support (x86_64, ARM64, RISC-V JIT compilers)

---

---

## Security

### Important Warnings
- **Experimental software** - Use at your own risk
- **Not Zcash** - Completely separate network with different blockchain
- **Incompatible addresses** - Never send Zcash to Juno or vice versa
- **Network isolation** - Different magic bytes prevent accidental Zcash connections
- **Limited testing** - Not audited for production use

### Best Practices
- Use strong RPC passwords (32+ characters)
- Restrict rpcallowip to localhost or trusted IPs
- Never expose RPC port to public internet
- Encrypt wallet: `./src/junocash-cli encryptwallet PASSPHRASE`
- Backup wallet
- Test on regtest/testnet before mainnet

### Known Considerations
1. Platform support includes x86_64, ARM64 (Apple Silicon M1/M2/M3/M4, etc.), and RISC-V architectures

---

## License

MIT License - See [COPYING](COPYING)

Based on:
- **Zcash** v6.10.0
- **RandomX** from Monero (BSD 3-Clause License)
- **Bitcoin Core** (original foundation)

---

## Credits

- **Juno Cash developers** -- This implementation
- **Zcash developers** - Base implementation
- **Monero developers** - RandomX algorithm
- **Bitcoin Core developers** - Original cryptocurrency

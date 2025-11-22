Juno Cash
===========

What is Juno Cash?
--------------

[Juno Cash](https://juno.cash/) is HTTPS for money.

Juno Cash is a ZKP (zero knowledge proof) based cryptocurrency where 100% of circulating coins are shielded by ZKP (specially Halo 2/Orchard).
For simplicity mining is does to transparent addresses which makes auditing of coin issueance more simple, however, before any new coins can be spent they must permanently enter the Orchard shielded pool, after which they cannot leave (Hotel California).
There are no trusted setups in Juno Cash, just pure privacy: transaction amounts and even the entire transaction graph are invisble.

Initially based on Bitcoin's design, Juno Cash has been developed from Zcash which was originally developed from
the Zerocash protocol to offer a far higher standard of privacy and
anonymity. It uses a sophisticated zero-knowledge proving scheme to
preserve confidentiality and hide the connections between shielded
transactions. More technical details are available in our
[Protocol Specification](https://zips.z.cash/protocol/protocol.pdf) which follows Zcash for now.

## The `junocashd` Full Node

This repository hosts the `junocashd` software, a consensus node
implementation. It downloads and stores the entire history of Juno Cash
transactions. Depending on the speed of your computer and network
connection, the synchronization process could take several days.

The `junocashd` code is derived from a source fork of [ZCash](https://github.com/zcash/zcash). The code was forked from version 6.10.0.
Zcash was forked from [Bitcoin Core](https://github.com/bitcoin/bitcoin), initially from Bitcoin Core v0.11.2, and the two codebases have diverged
substantially.

**Juno Cash is experimental and a work in progress.** Use it at your own risk.

#### :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16-week period. The automatic feature is based on block
height. Please check back regularly for new versions of the software as there is no auto update feature. This is on purpose to make Juno Cash voluntary and consensual software.

### Building

Build Juno Cash along with most dependencies from source by running the following command:

```
./zcutil/build.sh -j$(nproc)
```

Cross compile builds can be performed on Debian using the following commands:

```
# Linux build
./zcutil/build-release --linux -j $(nproc)

# Windows build
./zcutil/build-release --windows -j $(nproc)

# MacOS (Apple Silicon)
./zcutil/build-release --macos-arm -j $(nproc)


### Issues/Bugs/Features

Please file bug reports and feature requests in the Issue tracker on Github.

License
-------

For license information see the file [COPYING](COPYING).

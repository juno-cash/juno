Juno Cash
===========

What is Juno Cash?
--------------

[Juno Cash](https://juno.cash/) is HTTPS for money.

Initially based on Bitcoin's design, Juno Cash has been developed from Zcash which has been developed from
the Zerocash protocol to offer a far higher standard of privacy and
anonymity. It uses a sophisticated zero-knowledge proving scheme to
preserve confidentiality and hide the connections between shielded
transactions. More technical details are available in our
[Protocol Specification](https://zips.z.cash/protocol/protocol.pdf).

## The `junocashd` Full Node

This repository hosts the `junocashd` software, a Zcash consensus node
implementation. It downloads and stores the entire history of Zcash
transactions. Depending on the speed of your computer and network
connection, the synchronization process could take several days.

The `junocashd` code is derived from a source fork of [ZCash](https://github.com/zcash/zcash). The code was forked from version 6.10.0.
Zcash was forked from [Bitcoin Core](https://github.com/bitcoin/bitcoin), initially from Bitcoin Core v0.11.2, and the two codebases have diverged
substantially.

**Juno Cash is experimental and a work in progress.** Use it at your own risk.

####  :ledger: Deprecation Policy

This release is considered deprecated 16 weeks after the release day. There
is an automatic deprecation shutdown feature which will halt the node some
time after this 16-week period. The automatic feature is based on block
height.

### Building

Build Juno Cash along with most dependencies from source by running the following command:

```
./zcutil/build.sh -j$(nproc)
```

License
-------

For license information see the file [COPYING](COPYING).

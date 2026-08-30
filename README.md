# Badcoin Core

Project repository: https://github.com/badcoin-project/badcoin

Badcoin Core is the reference implementation of the Badcoin multi-algorithm blockchain. It is maintained by the badcoin-project community and connects to the Badcoin peer-to-peer network to validate blocks and transactions. It also provides command-line tools and an optional graphical wallet.

## Background

Badcoin has existed in two prior forms:

- **Badcoin (2017)** – A BitShares-issued token, not a standalone blockchain.
- **Badcoin (2018)** – A multi-algorithm Proof-of-Work blockchain launched as part of a promotional project by the Bad Crypto Podcast. Although development and chain activity later declined, the community preserved the source code and existing blockchain.

## Original Developer Acknowledgment

The 2018 Badcoin blockchain implementation was originally developed by an independent contributor known as ScriptProdigy. Although the original maintainer is no longer active in the ecosystem, their work remains the foundation on which the current modernized implementation is built. This project continues forward as a community-maintained effort.

## Badcoin Core Modernization

This repository contains the independently maintained modernization of the existing Badcoin blockchain implementation. The project is not affiliated with the original podcast creators or the original developer. Their initial contributions remain part of Badcoin’s history, while current development focuses on maintaining and improving the existing network and software.

## Badcoin Core v1.0.0.0

Badcoin Core v1.0.0.0 is the first modernized Core release maintained by badcoin-project.

This release continues the existing Badcoin blockchain. It is not a new chain launch and does not replace the existing chain history or balances.

The release includes:

- Updated Linux and Windows build workflows.
- A restored and qualified unit-test baseline.
- Badcoin-specific focused functional tests.
- Updated networking and standard transaction relay policies.
- Updated project and release documentation.

Before replacing an older Badcoin wallet download:

1. Back up the wallet data.
2. Completely shut down the existing wallet.
3. Store the backup in a separate, secure location.
4. Verify the new download and its published checksum before running it.

Official downloads will be published through the repository’s GitHub Releases page:

https://github.com/badcoin-project/badcoin/releases

Old wallet and download links should be replaced with the v1.0.0.0 release link after the release is published. Development builds and workflow artifacts should not be treated as final wallet releases.

## Mining Algorithms

Badcoin uses five independent Proof-of-Work algorithms:

- SHA256d
- Scrypt
- Groestl
- Skein
- Yescrypt

These are encoded directly into block `nVersion` bits. Relevant logic appears in `src/primitives/pureheader.h` and the associated proof-of-work selection code.

## License

Badcoin Core is released under the MIT License.

See `COPYING` or https://opensource.org/licenses/MIT.

## Building Badcoin Core

Basic Linux build:

```bash
./autogen.sh
./configure
make -j$(nproc)
```

Run on testnet:

```bash
./src/badcoind -testnet
```

Run the unit-test suite:

```bash
make check
```

Run the focused functional tests:

```bash
test/functional/feature_datacarrier.py
test/functional/p2p_message_size.py
```

Additional platform-specific build instructions are available in the `doc/` directory.

## Development Process

Development is coordinated through GitHub issues and pull requests:

https://github.com/badcoin-project/badcoin

The `development` branch is the primary integration branch and may not always be stable. Stable releases are prepared through dedicated release branches and identified by release tags.

Contributors should include appropriate tests with new or modified code. Consensus behavior should not be changed solely to satisfy tests inherited from upstream projects.

## Documentation

Repository documentation is available in the `doc/` directory. Some technical documents retain upstream terminology where it accurately describes inherited code or compatible tooling.

Additional Badcoin project documentation is available at:

https://badcoin.dev

## Testing

Badcoin Core includes unit tests and functional tests adapted to its existing formats, multi-algorithm proof-of-work behavior, relay policies, and networking behavior.

Changes should include appropriate test coverage and should preserve Badcoin-specific behavior rather than assuming Bitcoin defaults.

## Translations

Translation files and tooling currently follow the inherited upstream workflow where applicable.
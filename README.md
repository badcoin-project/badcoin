Badcoin Core – Community Integration / Staging Tree
Project repository: https://github.com/badcoin-project/badcoin

Badcoin Core is the reference implementation of the revived Badcoin multi-algorithm blockchain. It connects to the Badcoin peer-to-peer network to validate blocks and transactions and optionally provides a wallet and graphical user interface.

Background

Badcoin has existed in two prior forms:

Badcoin (2017) – A BitShares-issued token, not a standalone blockchain.

Badcoin (2018) – A multi-algorithm Proof-of-Work blockchain launched as part of a promotional project by the Bad Crypto Podcast. Although the chain later became inactive, the community preserved the source code.

Original Developer Acknowledgment

The 2018 Badcoin blockchain implementation was originally developed by an independent contributor known as ScriptProdigy. Although the original maintainer is no longer active in the ecosystem, their work served as the foundation on which this revived and modernized version is built. This project continues forward as a community-driven effort.

Badcoin Revival Project (2025–present)

This repository represents a fully independent, community-maintained modernization of the 2018 Badcoin blockchain. The project is not affiliated with the original podcast creators or the original developer. We acknowledge their initial contributions as part of Badcoin’s early history, but this effort is focused on long-term development, maintenance, and sustainability.

About This Release – v0.1.0 (Pre-Release)

This version marks the first pre-release of the revived Badcoin project. It prepares the codebase for a new testnet and lays the foundation for future development.

This release includes:

Updated build system

Removal of outdated or incorrect branding

Initial cleanup of legacy Bitcoin/Myriad components

Early documentation rebuilding

Preparation for restored unit and functional testing

v0.1.0 is not a stable or production-ready release. Breaking changes should be expected during active modernization.

Mining Algorithms

Badcoin uses five independent Proof-of-Work algorithms:

SHA256d

Scrypt

Groestl

Skein

Yescrypt

These are encoded directly into block nVersion bits. Relevant logic appears in src/primitives/pureheader.h and associated PoW selection code.

License

Badcoin Core is released under the MIT License.
See the file COPYING or https://opensource.org/licenses/MIT

Building Badcoin Core

Basic Linux build:

./autogen.sh
./configure
make -j$(nproc)

Run on testnet:

./src/badcoind -testnet

More detailed build instructions will be added as modernization continues.

Development Process

Development is coordinated through GitHub pull requests:
https://github.com/badcoin-project/badcoin

The main branch serves as the primary development branch and may not always be stable. Release tags will be created periodically to mark stable builds.

Documentation Status

The original Badcoin documentation inherited from Bitcoin and Myriad was outdated or incorrect, and much of it has been removed during modernization. Documentation is actively being rebuilt.

Currently:

Some files in the doc/ directory are incomplete or placeholders

Manpages have been removed and will be regenerated in a future release

RPC documentation reflects pre-modernization behavior

Wallet and developer documentation are under reconstruction

Updated documentation will be published at:
https://badcoin.dev

Testing

Unit tests and functional tests are being restored as part of the modernization effort. Contributors are encouraged to include tests with new or modified code. Legacy test scaffolding may not currently execute without updates.

Translations

Translation management currently follows the upstream Bitcoin Core workflow. Updated policies will be introduced later in the revival process.
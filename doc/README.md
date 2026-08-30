# Badcoin Core Documentation

This directory contains build, development, operation, and release documentation for Badcoin Core.

Badcoin Core is the reference implementation of the existing Badcoin multi-algorithm blockchain. Official releases are published at:

https://github.com/badcoin-project/badcoin/releases

Development builds and workflow artifacts should not be treated as final wallet releases.

## Running Badcoin Core

After extracting a release archive, use the appropriate Badcoin executable:

### Unix-like systems

- `badcoin-qt` — graphical wallet and node
- `badcoind` — headless node
- `badcoin-cli` — RPC command-line client
- `badcoin-tx` — transaction utility

### Windows

- `badcoin-qt.exe` — graphical wallet and node
- `badcoind.exe` — headless node
- `badcoin-cli.exe` — RPC command-line client
- `badcoin-tx.exe` — transaction utility

Before replacing an older wallet download, back up the wallet data, completely shut down the existing wallet, and verify the new download against its published checksum.

## Building

Platform-specific build notes include:

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [NetBSD Build Notes](build-netbsd.md)
- [OpenBSD Build Notes](build-openbsd.md)

These inherited build guides may describe older operating-system or dependency versions. Confirm dependencies before relying on them for a release build.

## Development and Release Documentation

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [DNS Seed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)
- [Fuzz Testing](fuzzing.md)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)

Some technical documents retain upstream Bitcoin Core terminology or links where they accurately explain inherited code, interfaces, or compatibility requirements. Those references do not identify this repository as Bitcoin Core.

## Additional Documentation

Additional Badcoin project information is available at:

https://badcoin.dev

Development is coordinated through the Badcoin repository:

https://github.com/badcoin-project/badcoin

## License

Badcoin Core is distributed under the [MIT software license](../COPYING).
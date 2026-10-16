macOS arm64 Build Instructions and Notes
========================================

Badcoin Core v1 is validated on native Apple Silicon through
`.github/workflows/macos-artifacts.yml`. That workflow runs on `macos-14`,
builds both core and Qt variants, runs unit and focused functional tests,
generates SHA-256 checksums, and smoke-tests the produced artifacts.

The workflow is the release-validation reference. Local builds are useful for
development, but a local build does not replace the workflow result.

Preparation
-----------

Install the Xcode command line tools:

    xcode-select --install

Install Homebrew, then the host tools used by the maintained workflow:

    brew install automake libtool pkg-config cmake

The maintained workflow also uses Python 3.11 with setuptools.

Build using depends
-------------------

From the repository root:

    make -C depends -j2
    ./autogen.sh
    HOST="$(./depends/config.guess)"
    CXXFLAGS="-D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION" \
      CONFIG_SITE="$PWD/depends/$HOST/share/config.site" ./configure \
      --prefix=/ \
      --disable-ccache \
      --disable-maintainer-mode \
      --disable-dependency-tracking \
      --disable-bench \
      --with-gui=qt5
    make -j2
    make check

The macOS arm64 workflow also applies the repository's arm64 test compatibility
step before configuration and runs:

    test/functional/test_runner.py feature_datacarrier.py p2p_message_size.py

Artifacts
---------

The maintained workflow stages two unsigned, checksummed tarball artifacts:

- `badcoin-macos-arm64-core`: `badcoind`, `badcoin-cli`, and `badcoin-tx`.
- `badcoin-macos-arm64-qt`: the core binaries plus `badcoin-qt`.

The smoke job verifies archive and per-binary checksums, executable permissions,
core version/help output, Qt launch/help behavior, and daemon/RPC startup.

Running
-------

The daemon is built at:

    ./src/badcoind

The Qt wallet is built at:

    ./src/qt/badcoin-qt

The default configuration file is:

    ~/Library/Application Support/Badcoin/badcoin.conf

This document does not claim Intel macOS release qualification for v1.

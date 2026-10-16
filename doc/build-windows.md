Windows x64 Build Notes
=======================

Badcoin Core v1 is validated for Windows x64 by
`.github/workflows/windows-artifacts.yml`. The maintained workflow cross-builds
both core and Qt variants on Ubuntu 22.04 with the x86_64 mingw-w64 POSIX
toolchain, generates SHA-256 checksums, and smoke-tests the core artifact on a
Windows runner.

The workflow is the release-validation reference. Other Windows build methods
may work for development, but they are not qualified v1 release paths unless
separately validated.

Cross-compilation on Ubuntu 22.04 or WSL
----------------------------------------

Install the required host and mingw-w64 tools:

    sudo apt update
    sudo apt install build-essential libtool autotools-dev automake pkg-config \
      bsdmainutils curl ca-certificates git python3 \
      g++-mingw-w64-x86-64 mingw-w64-x86-64-dev

Select the POSIX mingw-w64 compilers:

    sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
    sudo update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix

When using WSL, keep the source tree in the Linux filesystem rather than under
`/mnt/<drive>/`, because inherited dependency configure scripts can fail on
mounted Windows paths.

Core build
----------

    make -C depends HOST=x86_64-w64-mingw32 -j2 NO_QT=1
    ./autogen.sh
    CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" ./configure \
      --prefix=/ \
      --disable-ccache \
      --disable-maintainer-mode \
      --disable-dependency-tracking \
      --disable-tests \
      --disable-bench \
      --without-gui
    make -j2

Qt build
--------

    make -C depends HOST=x86_64-w64-mingw32 -j2
    ./autogen.sh
    CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" ./configure \
      --prefix=/ \
      --disable-ccache \
      --disable-maintainer-mode \
      --disable-dependency-tracking \
      --disable-tests \
      --disable-bench \
      --with-gui=qt5
    make -j2

Expected Windows x64 binaries are `badcoind.exe`, `badcoin-cli.exe`,
`badcoin-tx.exe`, and, for the Qt variant, `badcoin-qt.exe`.

The maintained smoke job verifies checksums, Badcoin version/metadata identity,
binary presence, and daemon/RPC startup on Windows.

The inherited 32-bit, Cygwin, MSYS2, and native Visual Studio instructions are
not v1 release-qualified and are intentionally not presented as supported build
paths here.

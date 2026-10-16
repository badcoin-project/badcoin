Building Badcoin Core
=====================

Badcoin Core uses the platform build guides under `doc/`.

Validated v1 build paths are:

- Linux x64 core: [doc/build-unix.md](doc/build-unix.md), with the maintained
  artifact workflow in `.github/workflows/linux-artifacts.yml`.
- Windows x64 core and Qt: [doc/build-windows.md](doc/build-windows.md), with
  `.github/workflows/windows-artifacts.yml`.
- macOS arm64 core and Qt: [doc/build-osx.md](doc/build-osx.md), with
  `.github/workflows/macos-artifacts.yml`.

The GitHub Actions workflows on `development` and `main` are the maintained
release-validation paths for these platforms. Legacy build methods may still be
present in the repository but are not release authority unless explicitly
recommissioned.

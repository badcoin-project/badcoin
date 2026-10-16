Gitian building
===============

Gitian files remain in this repository for legacy compatibility, but Gitian is
not the authoritative Badcoin Core v1 build or release path.

The maintained release-validation path is the GitHub Actions artifact workflow
set under `.github/workflows/`:

- `linux-artifacts.yml` — Linux x64 core build, checksums, tests, and smoke test.
- `windows-artifacts.yml` — Windows x64 core and Qt builds, checksums, and smoke tests.
- `macos-artifacts.yml` — macOS arm64 core and Qt builds, checksums, tests, and smoke test.

`contrib/gitian-build.sh` and the Gitian descriptors are optional legacy tools.
They are not release authority, and their signing paths must not be interpreted
as an implemented Badcoin signing or publication process.

Maintainers should follow [the Badcoin Core release process](release-process.md).
Final publication mechanics that are not yet automated remain pending completion
of the automated release workflow.

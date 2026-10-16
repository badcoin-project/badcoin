Repository Tools
----------------

### Developer tools

`contrib/devtools/` contains inherited and Badcoin-maintained development
utilities. Individual tools should be treated according to their current
repository usage rather than assumed to define release policy.

### Linearize

`contrib/linearize/` contains blockchain linearization utilities.

### QoS

`contrib/qos/` contains inherited Linux traffic-control helpers. Review their
network assumptions before use with Badcoin.

### Seeds

`contrib/seeds/` contains seed-generation utilities and documentation. There
is currently no authoritative automated Badcoin fixed-seed regeneration source;
see `contrib/seeds/README.md`.

Build and release-related tools
-------------------------------

### Debian

`contrib/debian/` contains inherited Debian packaging material. It is not the
authoritative v1 artifact workflow.

### Gitian

`contrib/gitian-build.sh`, `contrib/gitian-descriptors/`, and
`contrib/gitian-keys/` are legacy/optional Gitian material. They are not the
authoritative Badcoin Core v1 release path and do not establish current Badcoin
release signing infrastructure.

The maintained v1 build-validation workflows are under `.github/workflows/`:
Linux x64 core, Windows x64 core/Qt, and macOS arm64 core/Qt.

### MacDeploy

`contrib/macdeploy/` contains inherited macOS deployment helpers. The
maintained macOS v1 validation path is `.github/workflows/macos-artifacts.yml`.

### RPM

`contrib/rpm/` contains inherited RPM packaging material and is not a
release-qualified v1 artifact path.

Test and verification tools
---------------------------

### TestGen

`contrib/testgen/` contains utilities for generating test vectors.

### Verify binaries

`contrib/verifybinaries/verify.sh` is an inherited Bitcoin Core verifier that
downloads Bitcoin release signatures. It does **not** verify Badcoin Core
releases and is non-authoritative for v1. See
`contrib/verifybinaries/README.md` for the current Badcoin checksum and
verification status.

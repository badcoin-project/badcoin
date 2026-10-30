# Verify binaries

`contrib/verifybinaries/verify.sh` is an inherited Bitcoin Core verification
script. It downloads Bitcoin Core `SHA256SUMS.asc` files from Bitcoin release
infrastructure and therefore **does not verify Badcoin Core releases**.

Badcoin's current GitHub Actions artifact workflows generate SHA-256 checksum
files for the artifacts they build. The Linux x64, Windows x64, and macOS arm64
workflows verify those checksums again during their smoke-test jobs.

Badcoin Core does not currently have an authoritative signed-binary verification
workflow in this repository. Do not treat this directory, the inherited
`verify.sh` script, or Bitcoin Core signing keys as Badcoin release
verification.

Until a Badcoin-specific signing and publication workflow is implemented,
release verification is limited to the checksums produced and validated by the
maintained Badcoin workflows and any checksum material deliberately published
with a release.

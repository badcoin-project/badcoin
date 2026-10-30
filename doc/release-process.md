# Badcoin Core Release Process

This document describes the Badcoin Core release process used by the
badcoin-project community.

Badcoin Core releases continue the existing Badcoin blockchain. A release is not
a new chain launch and must not reset chain history, balances, consensus rules,
or network identity unless a separate consensus-change process explicitly says so.

## Maintained validation workflows

The maintained v1 release-validation workflows are:

- Linux x64 core: `.github/workflows/linux-artifacts.yml`
- Windows x64 core and Qt: `.github/workflows/windows-artifacts.yml`
- macOS arm64 core and Qt: `.github/workflows/macos-artifacts.yml`

These workflows run for pull requests and for pushes to `development` and
`main`. They are validation and artifact-building workflows; their artifacts
are not automatically final public wallet releases.

The current workflows provide, as implemented per platform:

- build validation for the listed platform/variant;
- SHA-256 checksum generation and verification;
- smoke-test validation of produced binaries;
- unit and focused functional tests where the workflow implements them.

Gitian files and `contrib/gitian-build.sh` are legacy/optional and are not the
authoritative v1 release path. `contrib/verifybinaries/verify.sh` is an
inherited Bitcoin verification tool and is not a Badcoin release verifier.

## Before a Release Candidate

- Confirm the target version in `configure.ac`.
- Confirm `CLIENT_VERSION_IS_RELEASE` is set correctly for the candidate.
- Confirm public-facing metadata uses Badcoin Core naming.
- Confirm release notes exist under `doc/release-notes/`.
- Confirm `doc/release-notes.md` links to the current release note.
- Confirm Linux x64, Windows x64 core/Qt, and macOS arm64 core/Qt validation
  workflows pass for the intended candidate commit.
- Confirm generated artifacts and checksums are from that commit.
- Review smoke-test results produced by the maintained workflows.

## Release Candidate identity

A release candidate is a specific candidate build and package set.

If an RC changes in a way that changes its binaries or packaged contents, the
changed candidate must receive a new RC designation. Do not silently replace or
mutate an already designated RC while keeping the same RC identity.

Documentation-only changes that do not alter an RC's binaries or packaged
contents still require review, but they do not by themselves redefine the
candidate binary identity.

## Chain Parameters

Only update chain-parameter checkpoints or assumptions when there is specific
evidence and review for the selected value. Chain-parameter changes are outside
ordinary release-documentation work and must be handled explicitly.

## Release Notes

For each release, maintain a versioned release-note file:

`doc/release-notes/release-notes-X.Y.Z.W.md`

Put the full release notes in that versioned file, then update
`doc/release-notes.md` so it links to the current release note.

## Candidate validation

Before approving a release candidate for promotion:

- Confirm the candidate branch contains only intended changes.
- Confirm the version and release notes.
- Confirm Linux x64 core validation.
- Confirm Windows x64 core and Qt validation.
- Confirm macOS arm64 core and Qt validation.
- Verify the workflow-generated checksums.
- Review implemented smoke-test validation.
- Confirm the artifacts correspond to the intended commit.

Development workflow artifacts are not final wallet releases merely because they
passed CI.

## Final release

Final publication mechanics are pending completion of the automated release
workflow. Do not claim that signing, tagging, GitHub Release creation, artifact
attachment, or public publication is automated until the repository implements
and validates those steps.

Before a final release is published, maintainers must at minimum:

- identify the exact approved candidate commit;
- confirm the final release is built from the approved candidate identity;
- confirm all maintained platform validations pass;
- verify checksums and smoke-test results;
- confirm the release notes match the approved contents;
- perform the still-manual publication steps under maintainer approval.

No signing infrastructure is claimed by this document.

Official releases, once deliberately published, are hosted at:
https://github.com/badcoin-project/badcoin/releases

## GitHub Actions release preparation

`Release preparation` is a manually dispatched, nonpublishing workflow. It
requires an immutable candidate commit SHA, a new `vX.Y.Z.W-rcN` designation,
and one successful artifact workflow run ID for Linux, Windows, and macOS.
It rejects duplicate run IDs, a source run from another workflow or commit,
unsuccessful source runs, incomplete artifacts, invalid checksums, missing
provenance, and an existing tag, branch, or release.

The platform artifact workflows retain a candidate SHA provenance record with
their artifacts. The preparation workflow checks that record, checksum files,
the expected core/Qt executable inventories, and the existing smoke-test gates
before producing a `badcoin-release-ready-staging` artifact. That artifact
contains an unsigned deterministic `release-manifest.json` and
`VALIDATION-REPORT.txt`. It does not create a tag, create a GitHub Release, or
publish files.

An approved maintainer may perform a dry run by manually dispatching `Release
preparation` with a candidate SHA and a deliberately unused RC designation,
then inspecting the uploaded staging artifact and comparing the manifest
checksum, commit, package inventory, and validation report with the selected
source workflow runs. A successful staging result remains review evidence only.

`Release publication` is a separate manually dispatched workflow. Repository
administrators must protect the `release-publication` GitHub environment with
the required reviewer approval before it can run. It has `contents: write`
only in its protected publication job; ordinary CI and preparation retain
read-only contents access. The maintainer must supply the exact successful
preparation run ID, release tag, manifest SHA-256, and the literal approval
value requested by the workflow. Publication rechecks tag/release collisions,
the preparation run identity, manifest checksum, candidate commit, and unsigned
status before it can create the release.

No cryptographic release signing is configured by these workflows. Staged and
published packages are explicitly labeled unsigned until separate signing
infrastructure is implemented and validated.

## Announcement and post-release

After deliberate publication, link to the GitHub release, remind users to back
up wallet data before upgrading, and state that the release continues the
existing Badcoin chain.

Do not silently replace published artifacts. A changed binary or package must
receive a new release identity appropriate to the change.

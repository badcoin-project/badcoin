# Badcoin Core Release Process

This document describes the Badcoin Core release process used by the
badcoin-project community.

Badcoin Core releases continue the existing Badcoin blockchain. A release is not
a new chain launch and must not reset chain history, balances, consensus rules,
or network identity unless a separate consensus-change process explicitly says so.

## Before a Release Candidate

- Confirm the target version in `configure.ac`.
- Confirm `CLIENT_VERSION_IS_RELEASE` is set correctly for the release candidate or final release.
- Confirm public-facing metadata uses Badcoin Core naming.
- Confirm release notes exist under `doc/release-notes/`.
- Confirm `doc/release-notes.md` links to the current release note.
- Confirm Linux and Windows build documentation matches the current workflows.
- Confirm CI passes on the release branch.
- Confirm generated artifacts are from the intended commit.

## Chain Parameters

Only update chain-parameter checkpoints or assumptions when there is specific
evidence and review for the selected value.

If updating chain parameters:

- Record the source command used to obtain the value.
- Prefer a block safely behind the current tip.
- Confirm the selected block is not orphaned.
- Review any `nMinimumChainWork` or `defaultAssumeValid` change carefully.
- Do not mix unrelated release-documentation cleanup with consensus or chain-parameter changes unless the scope is clearly documented.

## Release Notes

For each release, create a versioned release-note file:

'doc/release-notes/release-notes-X.Y.Z.W.md'

Put the full release notes in that versioned file.

Then update `doc/release-notes.md` so it links to the current release note.

Do not paste the full release note into `doc/release-notes.md`. That file is only the index.

## Release Candidate

A release candidate is a test release before the final release.

Before publishing a release candidate:

- Confirm the release branch contains only intended changes.
- Confirm the version in `configure.ac`.
- Confirm release notes are present and linked.
- Confirm Linux and Windows workflows pass.
- Confirm artifacts were built from the intended commit.
- Mark the release clearly as a release candidate.

Development builds and workflow artifacts are not final wallet releases.

## Final Release

Before publishing a final release:

- Confirm the final release commit.
- Confirm CI passes for that commit.
- Confirm expected Linux and Windows artifacts exist.
- Generate or verify checksums for release artifacts.
- Confirm the release notes match the final release.
- Create the final Git tag.
- Create the GitHub release from the final tag.
- Attach artifacts and checksums.

Official releases are published at:

'https://github.com/badcoin-project/badcoin/releases'

## Announcement

After the final release is published:

- Link to the GitHub release.
- Remind users to back up wallet data before upgrading.
- State that the release continues the existing Badcoin chain.
- Avoid price, investment, or speculative framing.

## After Release

After release publication:

- Confirm the GitHub release page is public.
- Confirm artifacts and checksums are downloadable.
- Watch for issue reports.
- Track follow-up fixes separately.
- Do not silently replace published artifacts. Publish a corrected release or follow-up release if needed.
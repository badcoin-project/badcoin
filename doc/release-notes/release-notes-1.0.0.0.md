# Badcoin Core v1.0.0.0 Release Notes

Badcoin Core v1.0.0.0 is the first modernized Core release maintained by the badcoin-project community.

This release continues the existing Badcoin blockchain. It is not a new chain launch and does not replace the existing chain history or balances.

Official releases are published at:

https://github.com/badcoin-project/badcoin/releases

Please report bugs through the repository’s issue tracker:

https://github.com/badcoin-project/badcoin/issues

## How to Upgrade

Before replacing an older Badcoin wallet:

1. Back up the wallet data.
2. Completely shut down the existing wallet.
3. Store the backup in a separate, secure location.
4. Verify the new download and its published checksum.
5. Install or extract the new Badcoin Core release.

The first startup may take longer while the node checks or updates existing data.

Development builds and workflow artifacts should not be treated as final wallet releases.

## Compatibility

Badcoin Core v1.0.0.0 is intended to continue using the existing Badcoin blockchain and wallet data.

Back up wallet data before upgrading. Downgrading after using a newer release has not been qualified and should not be attempted without a verified backup.

## Notable Changes

### Core modernization

This release follows months of modernization of the inherited Badcoin Core codebase.

- Replaced inherited OpenSSL `BIGNUM` / `CBigNum` arithmetic paths with explicit fixed-width internal arithmetic where appropriate.
- Reworked related hashing, proof-of-work, and block-work code to avoid legacy big-number interfaces.
- Removed obsolete Bitcoin/Myriad-era assumptions and corrected Badcoin-specific identity throughout the codebase.
- Modernized build and dependency configuration while preserving Badcoin’s existing chain behavior and compatibility requirements.
- Rebuilt project documentation and development hygiene around the maintained Badcoin Core repository.

These changes were foundational work, completed before release qualification, testing restoration, and artifact workflows.

### Release foundations

- Established Badcoin Core `v1.0.0.0` as the first modernized community-maintained release line.
- Added maintained Linux and Windows build workflows.
- Added Linux and Windows artifact generation.
- Updated project, build, and release documentation.
- Corrected inherited Bitcoin branding in public-facing Windows metadata.

### Test baseline

- Restored and qualified the inherited unit-test baseline.
- Adapted tests for Badcoin-specific address formats, key formats, rewards, mining behavior, and activation parameters.
- Added deterministic multi-algorithm regtest block generation.
- Added focused functional tests for data-carrier policy and P2P message-size limits.
- Enabled Linux CI to run the unit tests and focused functional tests before producing artifacts.

### P2P message-size limit

`MAX_PROTOCOL_MESSAGE_LENGTH` has been lowered from **32 MiB** to **8 MiB**.

This closes the long-standing Namecoin-inherited FIXME tracked in issue #1.

A measurement of Badcoin mainnet active-chain headers through height 1,865,375 found that the largest real 2,000-header message was approximately **0.1704 MiB**. The 8 MiB ceiling still accommodates a conservative synthetic batch of 2,000 headers at 4 KiB each while reducing the receive and denial-of-service ceiling by 75 percent.

Bitcoin Core’s 4 MiB value has not been adopted because AuxPoW parent-coinbase size remains unbounded. A lower limit should wait for byte-budget header batching or an appropriate AuxPoW size rule.

### Data-carrier policy

The default `-datacarriersize` and `MAX_OP_RETURN_RELAY` value is now **516 script bytes**.

This permits a single canonical push containing **512 metadata bytes**:

- `OP_RETURN`
- `OP_PUSHDATA2`
- two-byte payload length
- 512-byte payload

This is a transaction relay and mining-policy change. It is not a consensus soft fork or hard fork. Blocks containing larger data-carrier outputs remain valid to older nodes.

Operational notes:

- Older nodes using the previous default may reject these transactions from their mempool and decline to relay them.
- Once an upgraded miner confirms such a transaction, older nodes still accept the containing block.
- Operators can retain the historical limit with `-datacarriersize=83`.
- Operators can disable data carriers with `-datacarrier=0`.
- Only one `OP_RETURN` output per transaction remains standard.
- `OP_RETURN` outputs are unspendable and do not enlarge the spendable UTXO set.

The larger policy limit supports general on-chain metadata payloads without adding application-specific consensus rules to Badcoin Core.

## Credits

Badcoin Core v1.0.0.0 builds on the original 2018 Badcoin implementation and inherited open-source work from its upstream projects.

Thanks to everyone who tested builds, reported defects, operated network infrastructure, mined blocks, and helped preserve and revive the existing Badcoin network.

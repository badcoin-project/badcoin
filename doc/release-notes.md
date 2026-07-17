Bitcoin Core version 0.16.x is now available from:

  <https://bitcoincore.org/bin/bitcoin-core-0.16.x/>

This is a new minor version release, with various bugfixes
as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/bitcoin/bitcoin/issues>

To receive security and update notifications, please subscribe to:

  <https://bitcoincore.org/en/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/Bitcoin-Qt` (on Mac)
or `bitcoind`/`bitcoin-qt` (on Linux).

The first time you run version 0.15.0 or newer, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and there is no
automatic upgrade code from before version 0.8 to version 0.15.0 or higher. Upgrading
directly from 0.7.x and earlier without re-downloading the blockchain is not supported.
However, as usual, old wallet versions are still supported.

Downgrading warning
-------------------

Wallets created in 0.16 and later are not compatible with versions prior to 0.16
and will not work if you try to use newly created wallets in older versions. Existing
wallets that were created with older versions are not affected by this.

Compatibility
==============

Bitcoin Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

Bitcoin Core should also work on most other Unix-like systems but is not
frequently tested on them.

Notable changes
===============

Data carrier (OP_RETURN) default raised to 512-byte payloads
-----------------------------------------------------------

The default `-datacarriersize` / `MAX_OP_RETURN_RELAY` is now **516** script
bytes. That is the complete `OP_RETURN` scriptPubKey size, and it allows a
single canonical push of **512 metadata bytes**
(`OP_RETURN` + `OP_PUSHDATA2` + 2-byte length + 512 payload).

This is a **standardness / relay / mining policy** change only. It is not a
consensus soft fork or hard fork. Blocks containing larger data-carrier
outputs remain valid to old nodes.

Operational notes:

- Old nodes that still default to 83 will reject these transactions from their
  mempool and will not relay them. Once an upgraded miner confirms one, old
  nodes accept the block.
- Operators can keep the historical Bitcoin Core limit with
  `-datacarriersize=83`, or disable data carriers with `-datacarrier=0`.
- Only one `OP_RETURN` output per transaction remains standard.
- Resource impact versus the previous 80-byte default: up to ~435 additional
  transaction bytes per maximum-sized data-carrier output (~1,740 weight
  units). Fees, relay bandwidth, and storage grow accordingly. OP_RETURN
  outputs are unspendable and do not grow the spendable UTXO set.

This unblocks full on-chain metadata use cases (including BadPixies-style
payloads up to 512 bytes) without embedding any Pixie-specific consensus
logic in Core.

0.16.x change log
------------------

(to be filled in)

Credits
=======

Thanks to everyone who directly contributed to this release:

(to be filled in)

And to those that reported security issues:

(to be filled in)

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).

Badcoin Credits
=======
(not exhaustive or hierarchical)
- 8bitcoder
- nzsquirrell
- cryptapus
- jwinterm
- wlc
- ahmedbohdi
- voridor
- nickbo7

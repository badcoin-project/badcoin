Badcoin Core service configuration
==================================

Badcoin Core's current executable and configuration identities are:

- daemon: `badcoind`
- CLI: `badcoin-cli`
- Qt wallet: `badcoin-qt`
- configuration file: `badcoin.conf`

The files under `contrib/init/` are inherited service examples. Several still
use legacy Bitcoin filenames, service names, users, or paths. They are not an
authoritative v1 deployment interface and must be reviewed and adapted before
use. This documentation does not rename those files or imply that the legacy
samples have been recommissioned.

Configuration
-------------

Cookie authentication is supported when RPC credentials are not explicitly
configured. The authentication cookie is stored in the Badcoin data directory
by default and can be overridden with `-rpccookiefile`.

For a manually installed Linux service, use Badcoin identities consistently,
for example:

    Binary:              /usr/bin/badcoind
    Configuration file:  /etc/badcoin/badcoin.conf
    Data directory:      /var/lib/badcoin

The service account and directory layout are operator choices; the repository
does not currently define a single authoritative packaged init layout.

On macOS, the default user data directory is:

    ~/Library/Application Support/Badcoin

and the default configuration file is:

    ~/Library/Application Support/Badcoin/badcoin.conf

A service definition should invoke `badcoind`, not an inherited Bitcoin
executable name.

Legacy contrib/init status
--------------------------

The inherited `contrib/init/` examples may remain useful as templates, but
their Bitcoin-specific unit names, filenames, paths, or user names are legacy.
They should not be copied verbatim into Badcoin v1 release or deployment
instructions.

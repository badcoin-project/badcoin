Badcoin Core security policy
============================

Badcoin is experimental software. It secures real value only when people choose to trust it, so responsible disclosure of security issues is strongly encouraged and greatly appreciated.

Supported versions
------------------

Security fixes are provided for:

- The default branch of https://github.com/badcoin-project/badcoin
- Any actively maintained release branches listed in the repository tags or README

Older or unmaintained versions may not receive fixes. Running the latest release or the current development branch is strongly recommended.

How to report a vulnerability
-----------------------------

Please do not open public GitHub issues for security-sensitive reports such as:

- Remote code execution
- Wallet key / funds theft
- Consensus bugs or chain-split risks
- Denial-of-service vectors that are cheap to exploit

Instead, use one of the following private channels:

1. Use the “Report a vulnerability” option in the Security tab of the
   https://github.com/badcoin-project/badcoin repository, if available.

2. If that is not available to you, open a minimal public issue that does *not* include exploit details and clearly label it as “Security report – please contact me privately.” A maintainer can then coordinate with you to continue the discussion in a private channel.

Please include, when possible:

- A clear description of the issue and its impact
- Affected Badcoin Core version(s) and how you built/obtained them
- Reproduction steps or proof-of-concept
- Any logs or stack traces that help demonstrate the problem

We prefer reports in English, but will make a reasonable effort to understand others using translation tools.

Public disclosure
-----------------

We prefer coordinated disclosure:

- The issue is reported privately.
- A fix is designed, reviewed, and merged into the supported branches.
- New releases are prepared and announced.
- After users have had a reasonable time to upgrade, technical details may be shared publicly (for example in release notes or a post-mortem).

The exact timeline depends on severity and complexity. Critical issues affecting funds or consensus are treated with the highest priority.

Out-of-scope issues
-------------------

The following are generally not treated as security vulnerabilities:

- Misconfigured nodes, firewalls, or operating systems
- Compromise of systems not under the control of the Badcoin Core project
- Attacks requiring local malware, keyloggers, or physical access
- Generic denial-of-service via network bandwidth exhaustion
- Issues in third-party pools, explorers, wallets, or services that merely *use* Badcoin

If you are unsure whether something is in scope, please report it privately and we will triage it.

Attribution and thanks
----------------------

Researchers who report valid, previously unknown security issues and work with us on coordinated disclosure may be credited in release notes or documentation, if they wish.

There is currently no formal bug bounty program. Contributions are made on a best-effort, community basis.

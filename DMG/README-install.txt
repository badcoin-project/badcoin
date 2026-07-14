Badcoin Core wallet — macOS install instructions
=================================================

DMG: Badcoin-Core-20260714b.dmg  (~34 MB)
Build date: 2026-07-14
Signed by: Developer ID Application: Agile On Target LLC (U73U677CC4)
Notarized: not confirmed for this build (prior May builds were notarized)
Compatibility: Apple Silicon / Intel Macs
SHA-256: 260769d49814ebecd202cee7a27609caca797380bfe0935af644de0e47b6a19f

What's new vs prior DMGs
------------------------
- Send Note (public on-chain OP_RETURN, max 80 UTF-8 bytes)
- Sent payment history on Send (Date / Label / Note / Amount)
- Notes column on the Transactions tab
- Self-sends show in sent history
- Import Address on My Addresses (next to Remove)
- Fix for Pixie full-reindex hang after "Done loading"
- Send form layout: form at top, history fills the bottom

GitHub
------
Release download:
https://github.com/BaronZemodas/badcoin/releases/download/v0.16.3-note-20260714/Badcoin-Core-20260714b.dmg

Source branch (fork):
https://github.com/BaronZemodas/badcoin/tree/feature/send-note-history-20260714

PR into upstream:
https://github.com/badcoin-project/badcoin/pull/7

Install
-------
1. Double-click Badcoin-Core-20260714b.dmg to mount it.
2. Drag Badcoin-Qt.app into the Applications folder shown in the window.
3. Eject the DMG.
4. Open Applications and launch Badcoin-Qt.app.

If Gatekeeper blocks an unsigned/un-notarized build, right-click the app
and choose Open, then confirm.

If you previously ran an older Badcoin DMG
------------------------------------------
Your wallet data lives in ~/Library/Application Support/Badcoin/ and is
preserved across upgrades. No special migration needed. Always keep your
wallet.dat backed up before any major upgrade.

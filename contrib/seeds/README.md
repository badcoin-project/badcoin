# Badcoin fixed seeds

Badcoin Core compiles fixed fallback peer addresses into
[`src/chainparamsseeds.h`](/src/chainparamsseeds.h). These entries are used
only as a fallback when normal peer discovery does not provide usable peers.

The scripts in this directory are inherited seed-generation utilities. They are
not an authoritative Badcoin release procedure and must not be pointed at
Bitcoin seed sources.

There is currently no documented authoritative Badcoin fixed-seed regeneration
source or automated regeneration workflow in this repository. Maintainers must
not regenerate `chainparamsseeds.h` from an unrelated project's seed data.
Any future refresh needs a reviewed Badcoin-specific source of peer data and a
separate change explaining how that data was selected and validated.

The current compiled seed set remains authoritative until such a Badcoin-specific
refresh procedure is deliberately established.

# Disney native-resource continuation — 2026-11-06

Selected74b5 `disney_logical.h` covers data/status/control, both FIFOs and speed
analysis/last DAC, last_used/stereo/leader/state/interface detection. Original
`POD_Save_Disney` already saved these scalar histories but wrote whole `disney`,
including native mo/channel/interior leader addresses, before the relocation ID.
It also accepted out-of-bounds leader IDs. This is not missing sample history;
it is an original owner representation/resource correctness gap.

The original traversal now writes explicit scalar fields, with each DAC field
rather than padding/native pointers. Local read backing validates FIFO/leader/
state extents and canonical bools; interior leader is rebound only into destination
da[0..1]. Destination MixerObject/channel lifetime identities survive. Unknown
non-null source leader refuses publication. The one existing channel codec
continues to carry engine25 histories, no DI/DC record or duplicate validator.
Engine27/representation9 rejects older archives. No sound generation, PIC/time,
register replay or output publication during import. This is not full malformed
speed-clock arithmetic or audible/physical printer/device certification.

`tests/original-foundation/disney-owner-history.py` extracts actual owner codec
and backing. ASan/UBSan nonempty FIFO, analysis timestamp and leader1 roundtrip,
byte-stable resave, distinct destination owner identities, every truncated prefix,
invalid leader and unknown source leader checks pass. Channel lifecycle is a
stated double; active Disney guest/parallel runtime remains unqualified.
`proc-2d68ddb0f03b` exit0: this test, all three builds, diff check.
`proc-cf2a067e1deb` exit0: active Adlib/speaker/RTC two-consumer matrix
`fresh-root-matrix/run-zjcr4xde`; four combined debugger capture→fresh FILE/INLINE
held observations→explicit continuation→root42 E2Es
`e2e-milestone-uch4imqk` (speaker/timer, shared files, SB FIFO, new shell child).
Logs `$SAIVAGE_CARD_WORK_ROOT/disney-history-{lua,native,debug-no-lua}.log`.
Headers/toolchain unchanged from engine24. SHA256 Lua
`7958863a595edcafeb2369d0ac26bbcd07171a50f810c0ff83ec3dd4d6291737`, native
`2d0004074e46960fd044830314fe7621b15f8c8fdd19601680ab42a91e29db3b`, no-Lua
`e502fccb23e7bacb5f6da40871f5cf0e2d46f7c553bdfdcb76d72d89f53edffc`.
Exploratory missing test globs proc-797895bbfaa8/proc-c1b2dce15972 failed;
not check passes. No resource cleanup, service or publication performed.

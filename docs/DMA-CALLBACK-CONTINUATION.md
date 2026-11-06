# DMA callback/mode continuation — 2026-11-06

Selected74b5 channel record includes `transfer_mode`, `masked_by`,
`page_bank_increment_wraparound`, and `userData`. Original owner did not.
These fields affect read/write direction, mask callback behavior, PC98 bank
wrap and Sound Blaster callback card selection respectively. This is not a
reason to retain DA/DR/DM records or replay port programming. Direct append to
original DmaChannel SaveState/LoadState now preserves all four. Existing native
callback-role resolver, channel/width identity, controller flipflop, page/address/
count/flags, EMS mapping and DMA wrapping remain. Destination page-shift and
address-mask policy are not imported. Actor/mode bounds and SB callback indices
are checked before callback rebinding; there is no callback invocation during
installation. Engine26, representation9; archives from earlier engines rejected.
Failure disposes fresh target; this is not a transactional channel rollback.

`tests/original-foundation/dma-owner-history.py` compiles the ACTUAL owner body
and header fields. Nonzero card1/mode2/reissued mask/bank-wrap/current address/count
roundtrip, native policy preservation, every truncated prefix, invalid SB card2
refusal pass ASan/UBSan. Does not qualify active SB sound or PC98 execution.
`proc-cfba80923ed8` exit0 passed it, three configured builds, diff check and three
active Adlib/speaker/RTC captures to two fresh Lua/no-Lua consumers each, held
re-save then explicit guest continuation:
`$SAIVAGE_CARD_WORK_ROOT/fresh-root-matrix/run-i0pocc9p`.
Logs `$SAIVAGE_CARD_WORK_ROOT/dma-history-{lua,native,debug-no-lua}.log`.
Configured headers/toolchain unchanged from engine24.
Binaries SHA256 Lua `3861bb4ffa43360db295bced191283801e49e077400174da1cc53cca74d8c552`,
native `f47fa47c1dec975619db94d2e84fdad1a0bfe7e295f2b1cce883dc2edff07ee8`,
debug-no-Lua `f401ce2b524441081a0b2a389825307f6d4700ec4cc7d5aac04773d9ebca6d41`.

Exploratory proc-9cc55a5669f1 had nonexistent mixer test path after useful source
inspection; exit2, not a test pass. An initial actor upper bound GUEST was
corrected to REISSUE by reading actual enum before builds/tests. Existing runtime
capabilities and unknown resources unchanged; no cleanup or publication.

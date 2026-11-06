# Tandy DMA producer parity — 2026-11-06

Selected74b5fa198 versus029bd65 contains one ordinary-consumer correction in
`src/hardware/tandy_sound.cpp` beyond the removed logical codec additions:
`TandyDACGenerateDMASound` must split requests at `TDAC_DMA_BUFSIZE` and check
`transfer_done` between DMA reads. Original candidate still issued the entire
requested length into its fixed buffer. A synchronous terminal-count callback
can also finish DMA during the first chunk; subsequent output repeats the last
sample, not another DMA read. The direct selected loop is now in the original
producer. No new archive layout, API/CLI, codec, preflight, guest advance or
callback replay. Engine22/representation9 stays appropriate: owner bytes unchanged.

`tests/original-foundation/tandy-dma.py` extracts the actual producer into a
reduced DMA/mixer harness. ASan/UBSan checks zero,1,exact16,17,65 samples,
short read repetition and synchronous completion after the first chunk. This
qualifies request bounding and output count/hold behavior, not acoustic waveform,
all Tandy hardware or active saved Tandy continuation.

`proc-0fefc5a21d31` passes that test, both configured Lua/native builds and
`git diff --check`. Build logs at card work `tandy-{lua,native}.log`.
Current binaries SHA256 Lua
`013851b19076452e6c86704f5567c2c75dabc165028a71e760e2200cc1fa2ded`, native
`e52af9319a2f3cfa95334b66c11e24efeb434cada921c86dc8bc91391f45e124`.
Unchanged configurations/toolchain/headers are pinned in
`CMOS-CALENDAR-CONTINUATION.md` and earlier handoff.
`proc-58e5db25d6b0` passes all four combined FILE/INLINE fresh stopped restore
then explicit continuation/root42/disposal E2Es (`e2e-milestone-9iny7pr8`) and
actual native CMOS held/status/delay→Run/root42/mismatch/quit disposal
(`native-console-5tq8k7yy`) on these binaries. These are author checks, not
independent acceptance or whole-foundation closure.

Two attempted absolute/project-prefixed apply_patch operations returned no file
changes; exact scoped edit succeeded. An exploratory `git grep` used a nonexistent
serialize header and failed before later sequential inspection; not a passed
header check. Serializer actually resides in `include/dosbox.h`.

Reproduce: `python3 tests/original-foundation/tandy-dma.py`; existing configured
builds `make -C "$SAIVAGE_CARD_WORK_ROOT/build-{lua,native}" -j4` separately.
Set both DOSBOX_TEST_BINARY (Lua) and DOSBOX_TEST_NATIVE_BINARY (native) for
`fresh-e2e-milestone.py` and `DOSBOX_TEST_CONSOLE_FIXTURE=cmos fresh-native-console.py`.

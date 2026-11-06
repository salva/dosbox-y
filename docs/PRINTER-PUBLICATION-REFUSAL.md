# Ordinary printer publication refusal — 2026-11-06

`CPrinter::outputPage` now checks stdio write/close results for PNG and PS,
and flushes each multipage PS page before allowing `newPage` to clear raster.
On failure it closes its stream once, drops outputHandle, returns false and does
not run the configured host action. Existing newPage refusal keeps raster/cursor/
timeout. Partial host bytes may exist; no rollback, deletion or automatic replay
is promised. Source capture and Epson interrupted-job disposition are unchanged.

PNG's actual encode/write tail establishes libpng setjmp after all C++ vectors
are constructed and the page surface unlocked; an encode error destroys PNG
objects/closes the file and refuses publication. Errors from setup before this
tail and Windows physical printer API completion are not qualified by this change.
`finishMultipage` terminal trailer/close now emits an explicit failure diagnostic
and closes once; it cannot undo already published bytes or earlier page clear.
The actual terminal body is also exercised on full/null under ASan/UBSan.
Windows physical printer completion remains unqualified.

`tests/original-foundation/printer-publication.py` extracts the actual PS branch
and PNG publication tail. Real stdio `/dev/full` versus `/dev/null`, single and
multipage PS, action suppression, ownership and real libpng run under ASan/UBSan;
not a full-emulator failed-output integration test. `printer-page-refusal.py`
checks actual newPage refusal/success/reset with a publication double.

`proc-a25dbc35996d` && exit0: those PS/newPage tests, both configured builds and
five real native ordinary DOS LPT/ESC font-refresh/PS routes empty/missing/valid/
invalid/TTF (`printer-runtime-9i29szuu`). This predates PNG setjmp.
`proc-fc8c89ec645d` && exit0: final PS+PNG tests, both builds and all four real
fresh Epson dispositions (`printer-restore-72h3upwk`: unused/pending-page/
pending-ESC/completed-clean); diff check. Build logs card work root
`publication-{lua,native}.log`; config/toolchain unchanged from
INT10-APERTURE-FILELPT-CONTINUATION.md. Final binary SHA256:
Lua `67dd7a9ba0cbe6e14ddaf35a77207df27f7c68fdb45b98b2910ae01164fa47c8`,
native `0a8209032d0a86f966c97b9dc13eba1c81b1e9d791335d98ba2fd9866b738098`.

Final diagnostic-only successor `proc-52fb114e33be` && exit0 builds both and
checks all three production publication bodies. Final binaries Lua
`4355b6c192ea2be03baab06319b66688e72c37828c227a7e11591abf3c15c06e`, native
`52f6017bee331257f96da470899bfd91e8100d544cbf4e28452b7f03e1be7e36`.
Before that diagnostic-only successor, `proc-e53c2125820b` passed all four combined
fresh FILE/INLINE held/explicit continuation/root42 E2Es, native console and Render
SDL; trailing wrong script path failed (not a whole-chain pass). Corrected
`proc-c90e096e4d46` && exit0 passed actual exclusive Interactive/Bridge, RPC stop
conditions and Lua production-runtime tests using those pre-diagnostic binaries.
An intervening missing DOSBOX_TEST_BINARY invocation failed before emulator launch;
its command owner was killed after traceback because an Xvfb kept capture open.
No unknown process cleanup was performed.

No API/CLI changes or physical printer certification, no waiver of other output
or foundation requirements and no independent approval claimed.

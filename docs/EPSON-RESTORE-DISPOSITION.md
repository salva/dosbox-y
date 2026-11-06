# Epson interrupted operation restore — 2026-11-06

## Current owner decision and behavior

The requirement owner explicitly replaced full internal Epson checkpoint fidelity
with **destination-only hardware fault for an unfinished operation**. Clean idle
printers, including an allocated printer or prior completed page, remain usable.
This decision is not a waiver for unrelated parallel backends, normal printing,
font ownership, or ordinary refused-page preservation. Historical approved inputs
are unchanged. This document is the current §7/API/CLI/test successor for this seam.

`CPrinter::hasUnfinishedOperation` observes pending ESC/FS/variable command,
partial bitmap/DBCS character, nonblank page, or an open multipage output job.
Allocation and stale ACK alone do not mean unfinished. `SerializePrinter` in the
original common traversal records that classification and port data/control.
Capture does not reset, flush, emit, close, or mutate the source printer. The
fresh target installs a destination-only latch without constructing a printer,
loading fonts, replaying writes, or restoring a spool. Its saved Epson timeout is
removed from the reconstructed PIC queue without advancing time. Restore of an
already faulted target retains the fault in subsequent captures.

Fault raw LPT status is `0x40`: active-low /ERROR asserted, SELECT low, BUSY
asserted, ACK high, no paper-out claim. BIOS AH2 transforms its existing bits
(XOR 0x48); DOS LPT writes report failure before strobing. Software has to poll or
attempt printing; there is no unsolicited guest exception and no guarantee it
notices. INIT, form feed, timeout and screen-print cannot clear/bypass the fault
or emit a page. Recovery is a fresh ordinary launch; no in-session recovery API
is claimed. Clean restore remains usable and can print a new page normally.

CLI remains `-restore-state archive` before debugger/FILE/INLINE, with explicit
continuation only; native no-debug/no-Agent/no-Lua uses `-restore-console`.
There is no alternate printer save/load API. Engine15/representation9 rejects
older engine layouts; development archives from previous engines are not releases.

## Reproduction and exact applicability

At `/work/dosbox-y/original-savestate-restart`, `$W` is the card work root:

```
DOSBOX_TEST_BINARY=$W/build-lua/src/dosbox-x python3 tests/original-foundation/printer-restore.py
DOSBOX_TEST_NATIVE_BINARY=$W/build-native/src/dosbox-x python3 tests/original-foundation/printer-runtime.py
python3 tests/original-foundation/printer-font-lifetime.py
python3 tests/original-foundation/printer-page-refusal.py
python3 tests/original-foundation/render-sdl.py
```

`printer-restore.py` assembles the faithful small `fresh-printer.asm` in modes
unused/partial raster/partial ESC/completed page. Real debugger capture includes
repeated stopped saves and refused used-target loads; explicit source steps then
produce a new page in every mode (capture noninterference). Separate fresh
processes restore held, resave, and explicitly continue: pending cases verify
direct-port, BIOS and DOS fault with no new/changed PS output; clean cases print
new pages. This is actual emulator consumer evidence, not an extracted classifier.

- `proc-52ce87cf8ef2`: four modes including source continuation PASS,
  `printer-restore-u73yalb6`.
- `proc-f9d66ff790e0`: final fixture four modes PASS (`printer-restore-hie4v0az`),
  native sb-fifo held/status/Run/root42/mismatch/quit (`native-console-13znrm8x`),
  five ordinary native LPT/font/PS routes (`printer-runtime-nstfanev`), diff check.
- `proc-d672df3c3c94`: engine15 four combined debugger→fresh FILE/INLINE held
  restore→explicit continuation/root42/disposal routes PASS,
  `e2e-milestone-_zkkybjx`; speaker/timer, shared file, SB FIFO, shell child.
- `proc-1bd039912c25`: native build, five ordinary routes, actual font lifetime
  (24 reload/render/dispose cycles), actual refused-page branches ASan/UBSan PASS.
- `proc-5b3e4edeb367`: both final builds, actual Render codec/handler sanitizers,
  fresh-process real SDL dummy surface history/cursor/aspect hit-hit-miss PASS.

The source continuation fixture initially called a stack-based `send` after
capture with test-owned stack unsuitable for CALL, then had an incorrectly
encoded test jump. Failed attempts `proc-491a1acf7eb6`, `proc-f2f21c679a23`,
`proc-1d0931fa2a22`, `proc-43637b1cfbe4` are not production success evidence.
Inline port strobing and stopping the source assertion at actual page publication
corrected the fixture; no execution owner workaround was added. Earlier docpath
identity mismatch and BIOS status-polarity oracle failures remain historical.

Final generated configurations remain pinned in the handoff. GCC13.3/NASM2.16.01,
SDL2, optional Lua5.4 and native no-Lua, printer/FreeType/TTF enabled;
no OpenGL/avcodec. `proc-f9d66ff790e0` SHA256:

| Input | SHA256 |
|---|---|
| Lua binary | a3978f8f7118fbe4bb83a6bb050fd67c0233526642fa7ff99a885a1c5554d3e0 |
| native binary | a02471af7fea4a19a00f58835dcbd406fbeae4f14395559f7d514ddad6a9637f |
| Lua config.h | f3b2dd74f8cb9d7526b5bd8f976af4e04d416227c1dd5bc009dfcc7663e39975 |
| Lua config.status | 1da1be2e3e101ee762808fcffcbb1af26979ca703520056dab3b013687c65c60 |
| native config.h | 5a6293639a5798fdb3a5160d194f3529ca42ca09e7b0be24d0c11c0b298a132b |
| native config.status | 0910ab26954778fe5b46731d809819a646228bc5e2384b81691cb6dd26565f1c |

Independent successor assessment: [[card:card-n-c-l-b]] verified the current
engine29 binaries on production d69e0b40/candidate1bf357. Its
`proc-75ed86190171` independently passes all four actual fresh printer modes,
including source continuation, pending direct/BIOS/DOS fault/no changed PS and
clean new printing. Source review confirms allocation/stale ACK alone do not
fault; the classifier observes pending parser/bitmap/DBCS, nonblank page or open
multipage job. Raw0x40 and existing BIOS XOR0x48 are consistent active-low fault
projections, not a guest exception. Capture only observes; PIC timeout removal
and latch installation occur only at destination. No full interrupted-job
fidelity is claimed or required by the revised owner decision.

Author evidence is not independent assessment. This does not certify physical
printers, all write/close failure branches, active FileLPT host resources, or all
presentation/hardware modes. No service/publication/source switch/unknown cleanup.

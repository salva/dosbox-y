# Remaining foundation port: author handoff (2026-11-06)

This is a useful committed increment, **not whole-foundation or independent
acceptance**. Checkout `/work/dosbox-y/original-savestate-restart`, branch
`original-savestate-restart`. Selected74b5fa198 versus029bd65 remains the finite
scope; original4b7ffec is provenance. Fresh disposable progressive import supersedes
historical same-process/whole-preflight proposals; approved packets are unchanged.

## Delivered increments / API and CLI

* `70bbe958b53967aaba28c7dbe965a25c5f035854` (tree
  `cbc51d103b27472f89e3407dfde5a034591a9b70`): actual missing AH86 and VGA Pause
  guest-owned interruptible wait suffixes, one IRQ1 EOI/return, paired callback
  allocation/disposal. Direct/APM synchronous and non-VGA paths retained.
* `2d77f2f5a7feab8d92c05470d9e2a18554956673` (tree
  `24f074627ca64e7013f181b88a5532cb5f2834f8`): BIOS INT10/1100 font upload,
  active plane2 bytes/read mapping, fresh actual A000/BIOS text consumer; native
  console font/AH86/Pause selectors. Not pixel, PC98 or every TTF qualification.
* `e21978e7625abea21560043afb6649e0b6468b02` (tree
  `96881f39e656e694cd93654cb985c81d2a3386ed`): deterministic valid FAT12
  mounted-media regression. Initially failed; fixed by core owner, below.
* Current source increment on combined `ac455f1691af941a07f4749d3271ae3475a25cea`
  (tree `3964e9e4f538203de7164c150da82baa5a630e6e`): five VGA chained/slow/
  ET4000/unchained handlers now select independent write bank instead of read
  bank; printer uses independent FreeType library-owned face and retained copied
  display-font backing, disposes face before backing/library, handles absent
  face/empty font path and failed page allocation. Tests add active ET4000 banks,
  720/1440KB AH08 geometry, real LPT ESC font refresh/PS output and TTF route.
  This does **not** implement full active printer/UART/spool checkpoint state.

Engine12 / outer representation9 (prior increment receipts below name engine11). `-restore-state <archive>` attaches once to an
initialized-unused disposable process, before FILE/INLINE evaluation, held until
explicit Run/step. Used target load returns fresh-launch-required. No guest replay,
rollback, implicit CPU/PIC/TIMER advance or binary-equality compatibility rule.
Native no-debug/no-Agent/no-Lua uses `-restore-console`, not a Bridge listener.
Unsafe timed-callback refusal followed by held save is not nondebug integrated
autosave success. External boot/nonempty-device/shell-root/unrepresented lazy/LFN
limits remain. MIDI unqualified lower-priority follow-up.

## Core media correction and applicability

Engine10 valid FAT12 proc-a78e949881d0/run-ik_uiw24 failed repeated unchanged-Dos
capture (producer run-l8x4lkoh): global Opts scratch leaked geometry at Dos offsets
66396..66415/69800..69819. Separate fresh attempt proc-904de828d3d2 /
fresh-native-entry/run-z3ybbeeq failed BIOS image attachment/Bridge EOF before
any evaluator. Its sequential tail exit0 was not restore success. Earlier
nonformatted proc-ba4b18897250 and malformed proc-e0a0de55daea remain historical
failed inputs, not substituted as valid-medium evidence.

Core owner delivered `3b2e13a799087441654cc27e70397aebb807adca`, tree
`34580e34a598688bec628586b954e56a6b248843`, incorporated in combined HEAD above:
invocation-local zeroed Opts/padding; exact-path/geometry fresh BIOS image attachment
sharing locked DOS-owned handle; saved pending-change bool avoids AH06/root99.
Directly read core status9 reports independent k-b status13 approval; that review
was not independently repeated here. No unilateral core repair authored by this card.
Core proc-911fc1b98ada reports repeated FAT12/fresh held/INT13/root42 plus affected
font/waits/search/files/E2Es; proc-383318b6b3e8 reports native media and missing/
mismatch/fault disposal. Parent's failed SSH publication is separate, no remote
success or trust/push action authorized here.

## Exact current build handoff

Work root `/work/dosbox-y/.saivage/work/cards/card-n-c-l-a/` (denoted `$W` below).
GCC Ubuntu13.3.0-6ubuntu2~24.04.1; NASM2.16.01. Two out-of-tree configured builds:

* `build-lua`: `--enable-debug=heavy --enable-dosbox_agent --enable-sdl2
  --disable-opengl --disable-avcodec --enable-debug-lua`.
* `build-native`: `--disable-debug --disable-dosbox_agent --enable-sdl2
  --disable-opengl --disable-avcodec --disable-debug-lua`.

Both rebuilt with `make -j4` in proc-0fc90d83cac6 after final printer page guards;
logs `$W/build-{lua,native}-font-final.log`. SHA256 from proc-58e6997c14f8:

| Input | SHA256 |
|---|---|
| Lua binary | 592cf21b7fe06c8d9a59576eca388cd11c46ab174543b66f4c1a8ba9d6cb7739 |
| native binary | cc35079a003c66ce3d1e130901a19691f84f034f8e534e493c2f41324b0e3f97 |
| Lua config.h | f3b2dd74f8cb9d7526b5bd8f976af4e04d416227c1dd5bc009dfcc7663e39975 |
| native config.h | 5a6293639a5798fdb3a5160d194f3529ca42ca09e7b0be24d0c11c0b298a132b |
| Lua config.status | 1da1be2e3e101ee762808fcffcbb1af26979ca703520056dab3b013687c65c60 |
| native config.status | 0910ab26954778fe5b46731d809819a646228bc5e2384b81691cb6dd26565f1c |

`DOSBOX_TEST_BINARY=$W/build-lua/src/dosbox-x python3
 tests/original-foundation/fresh-root-matrix.py` runs the finite cases; optionally
set `DOSBOX_TEST_CASES=floppy-720-held,floppy-held,font-held,ah86-held,pause-held,bank-held,bank-service`.
ASM/BPB/FAT generation is deterministic, not checked-in runtime disk images.
`DOSBOX_TEST_NATIVE_BINARY=$W/build-native/src/dosbox-x python3
 tests/original-foundation/printer-runtime.py` exercises real DOS LPT/ESC close,
PS header/EOF/nonempty payload and guest completion witness with empty/missing
font paths and display TTF output. It does not compare rendered page pixels.
`fresh-native-console.py` accepts `DOSBOX_TEST_CONSOLE_FIXTURE=font|ah86|pause|sb-fifo|floppy`.
`fresh-e2e-milestone.py` runs debugger→fresh FILE/INLINE→explicit continuation.
Full-library Lua remains enabled, not sandboxed. Reduced ASan/UBSan probes:
`printer-font-lifetime.py`, `vga-banks.py`, `bridge-capture.py`; those are actual
extracted production bodies with doubles, not full integration certification.

## Current author evidence (raw logs retained, not versioned)

Tool process IDs below resolve under `work:///cards/card-n-c-l-a/processes/`;
these are author evidence, not independent acceptance or installed-host certification.

* proc-5dc01f53ae43 exit0, pre-page-guard binaries: matrix run-pxv8hnps720/1440KB,
  font/AH86/Pause/ET4000 mode13 service; four E2Es e2e-milestone-kyhl1y6s
  (speaker/timer,shared-file,SB-FIFO,new-shell-child), plus printer/VGA sanitizers.
* proc-709e134c6725 exit0: exclusive-runtime, rpc-runtime, production Lua,
  native font console and real printer PS output, same pre-page-guard input.
* proc-d4a46f3bd3c0 exit0: reduced render allocation/codec/stride (including
  CGA4/3270), SDL owner, completed-frame/clock, input vocabulary/cursor,
  eight mixer policies, BIOS attachment and RAM backing sanitizers. Not full
  backend/playback/physical input qualification.
* proc-0fc90d83cac6 exit0: both final builds, real printer empty/missing/TTF
  routes (printer-runtime-iv_p3clo),24 face factory render/dispose cycles under
  ASan/UBSan, five VGA handlers separate/alias byte/word/dword aligned/unaligned,
  diff check. proc-58e6997c14f8: repaired actual-body Bridge cleanup sanitizer
  test and final hashes. proc-3d6a297161b5 first exposed stale Bridge test doubles
  after successful printer and frame-observer tests; proc-d2ff41bbb37e exposed
  one remaining resume stub. Those compile failures were test drift, not passing
  tests or production repairs.
* proc-263e670f1003 exit0 final binaries: bank-held and bank-service actual
  fresh consumers, matrix run-sj9zef0z. Active mode13 ET4000 retains distinct
  read1/write2 banks and VRAM through held restore/resaves; explicit continuation
  writes through write2 while observing read1, changes read bank, verifies byte,
  exits42. Not VESA101 active-bank qualification.

Failed fixture development preserved: VESA101 bank variant root99
proc-73b455902177/run-9gs7p8vt, changed to mode13 (service pass
proc-6f4db18f75df/run-34yhitdy), not a production VESA repair. Initial active-bank
NASM `%if defined` syntax failure proc-a78ad3471a9c; wrong bank1 byte assertion
root99 proc-4dab1f3a8a27, corrected to expected byte0 without weakening the
separate read/write bank contract. Initial printer probes completed guest witness
but no PS until fixture explicitly sent form feed; not a production output fix.

Earlier engine10 evidence and failures remain in accepted status history; their
archive compatibility does not extend to engine11. In particular full39-case
proc-e806c340ce45, engine10 font32MB/native waits, and earlier engine9/frontend
chains are not claims of a current exhaustive matrix. Engine11 pre-bank/font
42-case proc-981e7d2d8f48 passed matrix/E2E/native media before wrong third
script path (overall exit2); corrected frontend chain proc-dc21464b381e passed.

## Text-layout and event-bound increment (2026-11-06)

On `00b8a982f56f327fa893e35f02710efed5fc7ecd` (tree
`c2f06b14d36e692fd65cf8274df0dae18d50d5a8`), the follow-on increment routes both
ordinary TTF setup and VGA drawing setup through `VGA_SetTextScanColumns` in the
owning VGA implementation. It applies columns*2 without replaying CRTC setup or
changing registers; does not yet supply the selected effective-address source tag.
Also retains selected serial event index bound (`serclassid<9`) before accessing
the nine-port array. No claim of UART state serialization follows from that bound.

proc-f6ddb1229c56 exit0 built both configurations, passed original-reference
scan arithmetic, all three real printer routes (printer-runtime-es9wm_34), and
four final combined E2Es (e2e-milestone-b_ah9nb3). This input included text-column
primitive but preceded serial bound. proc-27b8c4f4d5b8 additionally tests40/80/132
columns and unchanged hardware inputs in extracted actual primitive.
proc-9bc45b56a68f exit0 tests actual serial event valid/null/out-of-range dispatch
with ASan/UBSan then rebuilds both final binaries; logs
`$W/build-{lua,native}-serial-bounds.log`. Final binary SHA256:
Lua `e6783eaea6ba745b221e0295a5e15f05af4606b3d9f033d9a7109b3e278b9764`,
native `f4fff7cf26c4cea9250979b47c1c83bbfa8d5897759729982d4ecfa4ba535286`.
Generated config/header pins remain unchanged. Earlier table identifies previous
final-printer inputs, not these later binaries.

## Printer resolver increment (2026-11-06)

After `fa21dbfdec817d0bab24ab70606afd0db75a830a` / tree
`44c2bd7e0166f39db0fcb6ff7dd80190f16f1fdd`, the ordinary file/DBCS/system/embedded
fallback policy is factored into one `ResolvePrinterFont(FT_Library,Typeface)`
in printer.cpp, retaining ordering and independent face ownership. This is a
runtime owning-primitive extraction, not active printer checkpoint coverage or
a future-font-bank codec. The actual DOS LPT probe now exercises all six named
families and explicit valid/corrupt deterministic font directories as well as
empty/missing paths and TTF output. proc-477d0cac68a1 exit0 passes all five
routes with real PS output (`printer-runtime-8b56w1by`). proc-30826bee03c8 exit0
also passes independent native FreeType reload/render/dispose sanitizer test.
proc-74971276a636 built both configurations then failed before runtime because
the caller supplied DOSBOX_TEST_BINARY instead of DOSBOX_TEST_NATIVE_BINARY;
that invocation is not a runtime pass. Corrected commands above supply the latter.
Final binary hashes for resolver increment:
Lua `2a9cd6e28360a46bb60c76a05208b90e5a879285cc7b2a1226707910355986b8`,
native `d270cc6058658cac7e89dabd37a991ba18eda1c816f22ac0b9cb4145c7838ffa`.
Rebuild logs `$W/build-{lua,native}-printer-resolver.log`; earlier combined E2E
predates this behavior-preserving extraction and new family variations.

## UART owner increment (2026-11-06)

The next bounded implementation supplies actual dummy UART state in the common
engine, not a copied selected Prepared/catalogue path: `SerialPorts` before Pic,
logical registers/latches, diagnostics, effective timing and normalized FIFO
order (including empty retained head). Native ports/base/IRQ/backend bindings
are checked where consumed. Install does not call backend/register/IRQ methods;
PIC restores queued event timing separately. Non-dummy live backends remain
usable ordinarily but refuse checkpoint rather than pretending a UART-only
archive captures external transport. This matches the selected dummy-only bound;
no direct/modem/network checkpoint claim. Fresh failure remains disposal, not
rollback. Engine12 rejects engine11 archives rather than silently adding an owner.

Both configurations build in proc-5624e611ecaa; logs
`$W/build-{lua,native}-serial-owner.log`. Config/compiler pins above unchanged.
Lua SHA256 `a6251a016870d77aaa537122aa76bcebd1954f6eee53717a79ec1629307a1bd5`;
native `87d7adf5a69b73d7ae481ce0d042c18edb2f5b8cf872185b94985b3fbac476b1`.
`serial-fifo.py` extracts actual owner codec/ring operations and tests wrap,
empty-head, oversized/invalid/truncated input under ASan/UBSan. Real
`fresh-serial.asm` holds loopback byte55 in RX and byte66 in flight, nondefault
baud/FIFO/loopback/scratch registers; fresh restore must consume both in order
and return42 after explicit continuation. `DOSBOX_TEST_CASES=serial-held` matrix
passes proc-ae35ef6b9a42/run-m8txc0vb, as do FIFO sanitizer and four combined
fresh FILE/INLINE E2Es (`e2e-milestone-xcwvy1zh`). Native no-debug/no-Agent/no-Lua
`DOSBOX_TEST_CONSOLE_FIXTURE=serial` passes proc-56277014210f /
`native-console-qrat9ez3`: held/delay invariant, explicit Run/root42, wrong native
binding, three valid-CRC SerialPorts malformed payloads (short/presence/NaN),
quit disposal/no attach/no continuation and unchanged source archive.
These are author checks, not independent assessment or all serial backends.

Prior engine11 final resolver E2E passed proc-6201e7ab07f3 /
`e2e-milestone-2mv6k85k` on `a986e886fd64a9290362060150949c19dee6edd8`, tree
`8d89b322e792d59a1986c0cf71c491aaf2446e87`. It does not assert engine12
compatibility with engine11 archives.

## Remaining implementation and disposition obligations

Finite419-path/283-production ledger and105 helper bodies are mapping evidence,
not certification. Full source-grounded finite closure is still unfinished.
Actual remaining seams include selected effective scan/layout/output histories
and single text-column primitive (now routed through the VGA owner above, while
its selected effective-address history representation remains unfinished),
active parallel/printer parser/raster/timing/spool/future font-bank state
(no original owner serializer in those directories; dummy UART now implemented
above), and real provider/output
consumers. Ordinary independent printer face lifetime repair does not settle them.
Existing Render codec transports geometry/palette and reconstructs output; it
must not be described as completed canvas/history parity without implementation
and actual consumers. IDE's selected delta is catalogue-only, not active nonempty
IDE qualification. Broader hardware/composed and installed-host work remains with
parents, but this is not authority to silently parent-retain required missing
foundation behavior. Continue bounded implementation and escalate only genuine
material tradeoffs/unavailable decisions. No current external blocker demonstrated.

Independent affected assessment is arranged by parent. Preserve existing PID208/
port8080, old Xvfb584020/display106, uncertain inherited processes/archives/damaged
builds and caches; no cleanup established. All new commands collected, test-owned
raw evidence retained. No source switch/rebase/reset/service/publication action.

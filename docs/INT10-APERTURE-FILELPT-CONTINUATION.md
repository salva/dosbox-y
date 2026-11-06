# Int10, VGA aperture and FileLPT continuation — 2026-11-06

Author implementation/verification, not independent whole-foundation acceptance.
Engine20/representation9; original owner traversal, no Prepared/replay/rollback.

## Additional engine21 variations (2026-11-06)

`proc-5d54012e374f` && exit0 extends actual FileLPT fixture to a 100ms
pending timeout/addFF/cp437: fresh held comparison remains unchanged, explicit
continuation writes X/addLF then waits five BIOS ticks; current external append
survives and exactly one formfeed follows, root42. Reproduce with
`DOSBOX_TEST_FILELPT_WAIT=1 DOSBOX_TEST_FILELPT_TIMEOUT=100
DOSBOX_TEST_FILELPT_FLAGS='addLF addFF cp:437'` and the usual Lua binary env.
`proc-66bf2b191bf2` && exit0 separately revalidates actual exclusive
Interactive/Bridge, RPC stop conditions, full Lua production runtime and
FileLPT99/cp437 on engine21 binaries. Full output retained at
`work:///cards/card-n-c-l-a/processes/proc-66bf2b191bf2/stdout.log`.
These do not certify multiport/Windows behavior.

Actual missing/FIFO/directory target refusals are now exercised by
`DOSBOX_TEST_FILELPT_REFUSAL=missing|fifo|directory filelpt-restore.py` with the
same binary env. `proc-17ad4237f638` && exit0 preserves producer bytes under a
separate test-owned pathname, verifies no replacement creation or modification,
nonzero FileLPT install failure, terminated process, closed control transport and
unchanged source archive. DEBUG_Init connects the control transport before
installation; it is not a published usable evaluator. Initial
`proc-4fef622afc22` failed a too-strong no-connection assertion on this same
missing-target route; its native log shows the correct FileLPT install failure,
not successful restore. Corrected assertion observes terminal/closed transport,
not absence of initialized transport. No production change was needed for these
refusals. This does not prove every late cleanup failure or every resource close.

## Delivered owner state

`src/ints/int10_modes.cpp` now transports all 22 mutable mode table spans,
including sentinel cells, and CurMode null/table-index/detached identity through
`POD_Save/Load_Int10_Modes`, called from the original Int10 component. Detached
values use destination-owned backing. No mode switch/BIOS/port replay. The
ASan/UBSan extraction `int10-modes.py` covers every alias/sentinel, detached/null
and truncated payload. This is not guest VESAMOED mutation certification.

`src/hardware/vga_memory.cpp` preserves actual A0–BF page handler choices with
32 destination-local VGA handler identities; zero delegates RAM/ROM/other to
Memory. This repairs mode13 fresh restore retaining initialized text aperture
when the original Memory handler table emitted unknown0xff for chained VGA.
No VGA_SetupHandlers replay or core owner modification. The guest `fresh-dac.asm`
holds after DAC components41/42 and A000:0123=5a, then explicitly continues with
component43/readback, checks restored aperture, writes0124=6b and returns text/
root42. `dac-restore.py` also exercises text DAC continuation.

`src/hardware/parport/filelpt.{h,cpp}` transports register/ACK/autofeed state,
lastChar/lastUsedTick, shared 105-byte lookahead and count (0..100), codepage,
open target and consumed native filetype/base/IRQ/timeout/addFF/addLF/actions.
FileLPT is in common traversal before Pic; the parallel event callback is locally
relocated. Actions must match native configuration; they are not run by import.
Open targets must already exist and be regular: open append/no-create/no-truncate,
fstat the actual descriptor, continue at current EOF. Capture is observational;
no host content rollback, replay or saved-offset rewind. Imported streams close
silently on held/failure disposal until explicit Putchar resumes ordinary ownership.
Ordinary configured actions remain enabled after continuation, not globally removed.
Nonregular open streams refuse checkpoint; ordinary printing remains available.

`filelpt-restore.py` exercises real guest strobe/ACK, acquired append stream, a
host EXTERNAL append after save, held fresh restore/resaves and explicit X
continuation preserving current bytes then addLF/root42. PREFIX=99 with cp:437
reaches the shared lookahead boundary100. ACTION=1 configures a harmless quoted
`touch` command and asserts its ordinary close effect exists after continuation.
This assertion is not an independent proof of timeout scheduling or of every
host-action failure/disposal route. Windows compilation, capture-path variation,
nonregular/failure/refusal matrix and multiport shared lookahead remain unqualified.

## Checks and exact inputs

Work root `$W=/work/dosbox-y/.saivage/work/cards/card-n-c-l-a`.
Both configured builds unchanged from handoff: GCC13.3, NASM2.16.01, SDL2,
Lua5.4 heavy debugger/Agent versus no-debug/no-Agent/no-Lua native; FreeType/TTF,
no OpenGL/avcodec. Generated inputs remain:

| input | SHA256 |
|---|---|
| Lua config.h | f3b2dd74f8cb9d7526b5bd8f976af4e04d416227c1dd5bc009dfcc7663e39975 |
| native config.h | 5a6293639a5798fdb3a5160d194f3529ca42ca09e7b0be24d0c11c0b298a132b |
| Lua config.status | 1da1be2e3e101ee762808fcffcbb1af26979ca703520056dab3b013687c65c60 |
| native config.status | 0910ab26954778fe5b46731d809819a646228bc5e2384b81691cb6dd26565f1c |
| Lua binary | d65a3226af8e9405305bf595aa26cb7acd581968564557b5dfbc645a16f3972a |
| native binary | eae70b76460858af7c31a1ad7aacab564a8c6159656e055b5fc3d18243367a60 |

- `proc-ce3f759ccca1` && exit0: both builds (logs `$W/engine20-{lua,native}.log`),
  active FileLPT+cp437+prefix99, native held/delay/status/Run/SB-FIFO/root42 and
  mismatch/quit disposal (`native-console-tbdlcjx4`), hashes/diff check.
- `proc-e79a09493fee`: corrected quoted action test and effect assertion PASS
  (`filelpt-restore-8vk5ym0i`). Earlier unquoted action pass did NOT assert that
  particular action path; do not treat it as action verification.
- `proc-5a66b1627984` before action-string codec addition: Int10 sanitizer,
  full default fresh matrix (`$W/engine20-matrix.log`) and all four combined
  debugger→fresh FILE/INLINE held→explicit Run/root42/disposal E2Es
  (`e2e-milestone-aw1vs92l`) PASS. Its trailing native test failed KeyError
  DOSBOX_TEST_BINARY, corrected by proc-ce3f759ccca1; overall exit1 retained.
- `proc-2286b9ccf26d` earlier plain and prefix99/cp437 FileLPT PASS;
  `proc-f61007d4a5ea` four Epson modes/Render SDL/VGA filter+DAC sanitizers/
  real exclusive/RPC/Lua production runtime PASS. These predate action binding.
- Mode13 held-memory mismatch `proc-2bbcde943c82` was actual aperture loss,
  diagnosed `proc-40e05a579a92`, repaired and checked in `proc-db792010ae48`
  and `proc-898d9a11f691`. Not a discarded oracle or passing earlier input.

Reproduce with DOSBOX_TEST_BINARY=$W/build-lua/src/dosbox-x and python3
`tests/original-foundation/{int10-modes,dac-restore,filelpt-restore}.py`;
DAC graphics uses DOSBOX_TEST_DAC_GRAPHICS=1, FileLPT variation uses
DOSBOX_TEST_FILELPT_PREFIX=99 DOSBOX_TEST_FILELPT_FLAGS='addLF cp:437'
DOSBOX_TEST_FILELPT_ACTION=1. Native console needs BOTH DOSBOX_TEST_BINARY and
DOSBOX_TEST_NATIVE_BINARY=$W/build-native/src/dosbox-x.

## API/CLI §7 successor and remaining work

No new public command or changed evaluation semantics. Existing -restore-state,
-restore-console, FILE/INLINE save/restore and explicit Run rules are retained.
Used targets require fresh launch; stopped observation causes no CPU/PIC/TIMER
advance. Engine17/19/prior engine20 development archives do not establish current
layout compatibility. Historical approved packets/Astra remain unchanged.

Int10/aperture/FileLPT receipts close specific omissions, not the whole finite
selected ledger. Native-format-dependent VGA TempLine/text nibble recurrence,
uniform/debug/BIOS scratch writers and applied write-visibility still require
source-grounded disposition/implementation and meaningful affected consumer
qualification. Ordinary Epson output write/close accounting is not waived by
interrupted-job disposition. Further bounded foundation work and final combined
input/finite disposition/independent assessment remain; no external blocker or
exhausted implementation route is claimed.

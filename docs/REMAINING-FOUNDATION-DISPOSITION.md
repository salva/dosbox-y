# Selected remaining foundation: §7 finite implementation disposition

2026-11-06. Applies to selected `74b5fa198e619686d506d152b0edcd1d52d5bd0f`
versus `029bd65c5dcc7cdc7f2e14e35278620d9707d969`, candidate production
`d69e0b40a9a4459a29c05d96a457438a6ee72e63`, tree
`dc9b3b7eab16a3923f9c40ea03595e80916401f1`, engine29/representation9.
Original4b7ffec is provenance, not the port baseline. This document reconciles
the finite map with implementation and evidence, not an independent acceptance
or certification of every guest/device/backend. Approved historical packets and
Astra design findings remain unchanged.

## Implementation disposition

The 419-path census and 283-production-path associations in
`SEMANTIC-PORT-MAP.md` are source-discovery aids. Their selected duplicate record
headers, profile inventories and Prepared catalogues are not additional APIs to
import. The following is the disposition of their actual behavior by owner.

| Map group | Current disposition / detail |
|---|---|
| Exclusive execution, context, result, lifetime | Retained original common owner and thin Interactive/Bridge/RPC/Lua consumers, no second decoder/controller. Unlimited Run, stop_any/time/text/predicate and unrelated native operations preserved. Actual current consumer chain below. |
| Persistent Interactive and full worker Lua | Retained distinct capability/context/lifetime semantics: persistent evaluation/sink versus worker synchronous execution and callbacks; full libraries/C require/file/code/error/close, not sandbox. Original standalone deferred intent/finalizer boundaries retained. |
| Unified save API and frontends | Original common capture/install and original per-owner traversal; stopped save, file/slot/menu/hotkey/GUI/native/Bridge/Lua/checkpoint converge there. Used target requires fresh launch. Core corrections and original-foundation tests remain reference, not a parallel checkpoint engine. |
| CPU/FPU/paging/memory/DOS/files/media | Delegated core accepted corrections retained, including later core media `3b2e13a799087441654cc27e70397aebb807adca` incorporated by ac455f. `FRESH-RESTORE-CORE-CORRECTIONS.md`; no unilateral redesign. Files aliases/shared handles/current-content cursors/search, HMA/EMS/XMS/media geometry useful variations in matrix. No host-content rollback/destructive reopen. |
| AH86/Pause | Newly ported guest-owned wait continuation, paired native resources; direct/APM/non-VGA behavior retained. `REMAINING-FOUNDATION-HANDOFF.md` first increments and actual active native/matrix cases. |
| Keyboard/gameport/mouse/native input | Original owner state and mapped native dispatch retained; pending D1/AUX and timed/untimed gameport actual continuations qualified. No physical-input-state certification. |
| PIT/PIC/DMA | Original clock/counter/event queue traversal retained; DMA logical actor/userData and mode/mask/wrap native rebind repaired, without callback invocation. `DMA-CALLBACK-CONTINUATION.md`; actual timer/DMA/GUS/PIC tests, active wait/speaker consumers. No tick/event service during install. |
| CMOS | Calendar/alarm/native clock/pending BIOS sync added to original Cmos owner. `CMOS-CALENDAR-CONTINUATION.md`; locked leapday fresh/native consumer, not all host-synchronized IRQ behavior. |
| Serial/parallel/FileLPT | Dummy UART FIFO/register/timing owner added; other live transports explicitly refuse checkpoint without removing ordinary operation. `INT10-APERTURE-FILELPT-CONTINUATION.md`: active buffer/lookahead/cp437/ACK/control/time and existing append-only regular target, current EOF/external append, held no-write/disposal. No destructive recreate. Actual native route. Event bounds and exact close ownership repaired. |
| Epson/font/output | Requirement owner superseded full interrupted parser/page/font/timer reconstruction with hardware-fault disposition ONLY for unfinished printing. `EPSON-RESTORE-DISPOSITION.md` records authority and actual mid-ESC/page fresh behavior; clean/used-idle printer remains usable. Independent FreeType library/face/copied backing lifetime and resolver retained. `PRINTER-PUBLICATION-REFUSAL.md`: truthful PS/PNG detected failures preserve page/cursor/timer, not physical printer completion. |
| Adlib/DBOPL/CMS/Tandy/Disney/speaker/mixer | Original per-owner representations retain active synthesis histories. Added Adlib PCM gain/DC/poll clocks, speaker edge/gate/queue history, mixer guest-time filter/interpolation/work/cursors, bounded Tandy DMA producer, destination-local channel/DMA identities for Disney/Tandy. Dedicated continuation docs and actual-chip next-sample comparisons; no audible/acoustic certification from fixtures. |
| MPU/SB/GUS | `MPU-SB-OWNER-CONTINUATION.md`, `GUS-NATIVE-ADDRESS.md`: parser position/count/length moved from static to original owner; no source native channel/DMA/DRAM addresses transported; logical DMA role/local resources retained. Upstream GUS0.32ms/PIC and actual-port SB forwarding regressions retained. Physical MIDI remains explicitly lower-priority unqualified. |
| VGA memory/registers/fonts/Int10 | Original draw/register/device owners and local bindings retained. Independent write banks,52 handlers/null, bounded TempLine, hretrace/DAC latches, Int10 mutable mode spans/identity and32 aperture roles added/corrected. `INT10-APERTURE-FILELPT-CONTINUATION.md`, corresponding VGA/DAC/relocation docs. No BIOS/SetupDrawing replay. |
| VGA effective scan and producers | `VGA-APPLIED-SCAN-DISPOSITION.md` traces selected scan/layout/address/row/split/schedule/panning/visibility recipes to applied original draw POD; no duplicate recipe codec. Upstream CGA4/3270 stride explicitly preserved against selected omission. Native text recurrence and alignment-safe cursor loads corrected; SMZX/font/bank/VESA actual consumers supplement arithmetic. BIOS-logo transient unknown handler refuses instead of silently importing pointer. |
| Render/SDL/completed output | `RENDER-CONTINUATION-CORRECTION.md`: original owner now transports cache/comparison/dispatch/line phase/frameskip/acquisition/cursor and RGB canvas, destination-local output. Actual acquired→cache-hit→miss with repeated aspect rows and fresh SDL tested; no second acquisition/double advance or guest draw/time. Non-direct/locked/blit/xbrz/post-aspect routes refuse restore without deleting ordinary runtime routes. |
| Ordinary screenshot | `ORDINARY-SCREENSHOT-PORT.md`: final source reconciliation found and repaired missing ordinary `CAPTURE_AddImage` shared-writer delegation; all bpp/doubling decoded-pixel tests, failure cleanup/close-only notification. Headers alone had not delivered this consumer. |
| PCI/IDE/Voodoo | Selected IDE delta is IO catalogue only, PCI dormant codec is match-only config/CF8, not active-device parity. Original IO owner retained, duplicate registry discarded. Selected ordinary Voodoo presence versus PCI-command/callout lifetime fix now ported and sanitizer tested (`VOODOO-CALLOUT-PARITY.md`); discarded dormant Prepared codec is not active accelerator qualification. |

## API / CLI / safety contract

No new public API or CLI in these remaining corrections. `-restore-state <archive>`
consumes once into initialized-unused disposable fresh process, stopped before
FILE/INLINE evaluation. Native no-debug uses `-restore-console`; wrong route is
not a restore pass. Explicit Run/step only advances guest. Representation,
effective hardware/immutable inputs/native binding and justified ABI compatibility
are checked, not textual config/binary/backend equality. Failed progressive
install disposes destination, never rolls back/replays. Capture/publication,
close, durability and cleanup effects remain separate; unknown close is not
success. No global Prepared/whole-machine preservation/second validator/codec.

Unsafe timed callback refusal followed by held save is qualified; it is NOT
nondebug integrated autosave success. External boot/nonempty unsupported external
devices/shell-root/unrepresented lazy/LFN limits remain explicit. Arbitrary
active IDE/Voodoo, all extension hardware, all pixel/overlay/BIOS XOR/output
backends, all audio rates/physical playback and composed hardware are not
certified by this bounded foundation. Parent root/n retains exhaustive/composed
qualification and root/i installed-host/MCP/Saivage; ordinary capabilities were
not removed to achieve these limits. MIDI explicitly remains follow-up. These
are scope/qualification limits, not approval of untested behavior or permission
to silently drop a discovered required defect.

## Exact verification handoff

`ORDINARY-SCREENSHOT-PORT.md` pins latest three binaries/build logs and fixture
reproduction; config.h/config.status/toolchain pins in historical handoff remain
unchanged (GCC13.3/NASM2.16.01/SDL2/FreeType/printer/TTF; no OpenGL/avcodec).
Lua heavy-debug+Agent+Lua5.4, native no-debug/no-Agent/no-Lua and debug-no-Lua
profiles all rebuilt proc-1312607d6f0e exit0. Production source tree is above;
subsequent disposition documentation changes do not alter binaries.

* proc-8efe756fb980 exit0 all-AND chain on latest production: actual exclusive
  Interactive/Bridge, RPC (`rpc-runtime/run-g2rhp39o`, including
  REAL_RPC_STOP_CONDITIONS_PASS), full production Lua callback/file/code/save
  (`lua-production-runtime/run-h9jk4ic6`), debug-no-Lua unavailable Lua/native
  steps. Partial stdout was searched, not mistaken for complete inline output.
* Latest four combined debugger→fresh FILE/INLINE held observation/save/host-delay
  →explicit continuation→root42 E2Es: `e2e-milestone-ll5sr1xl`, all pass in
  proc-aea5c8243101. Subsequent wrong native producer failed; corrected native
  SB FIFO held/Run/root42/mismatch/quit disposal proc-9c61b076e423 exit0.
* Engine29 ed293 prior to Voodoo/screenshot ordinary-only fixes: all47 meaningful
  device/file/ancestry variations to TWO Lua/debug-no-Lua fresh consumers each,
  proc-154819bb76b1 exit0 `fresh-root-matrix/run-h96incvf`. This is not a claim
  that all47 were rerun after those two ordinary fixes.
* Current ordinary owner sanitizers, three builds, combined E2Es and bank/VESA
  regression details and failed fixture qualifications in the two final port docs.

Author delivery is not independent acceptance; the independent assessment below
now supplies the bounded recommendation, not broader hardware qualification. No service/publication/adoption/source switch/unknown cleanup occurred.
Preserved unrelated pycache remains; inherited resource effects/ownership remain
as recorded in current status and accepted history.

## Independent bounded acceptance recommendation

[[card:card-n-c-l-b]] independently recommends acceptance of this complete bounded
disposition at candidate `1bf357c90c10daad3d9e1399037c5fd623ceba68`, tree
`16a8510b832157f51f22d10692aecf7d7fdc4201`, production d69e0b40 above.
This includes the explicitly revised Epson requirement, not full interrupted-job
fidelity. No production repair or independent rebuild was performed. All three
reused binaries and generated headers/configuration were independently hashed;
the assessor current status records exact pins and qualifications.

Independent terminal evidence under `work:///cards/card-n-c-l-b/processes/`:
`proc-75ed86190171` passes actual Epson unused/partial-page/partial-ESC/completed
fresh consumers, acquired Render hit-hit-miss/aspect owner and separate-process
SDL witnesses, four combined FILE/INLINE E2Es and exclusive/RPC/full Lua routes.
`proc-22bbb7ee7660` passes all47 matrix cases to both Lua and debug-no-Lua fresh
consumers on final binaries. `proc-9bff32e6e728` passes affected synthesis, VGA,
FileLPT, screenshot, Voodoo, publication and actual native-console checks.
DMA/RTC/Disney/Tandy/mixer/Adlib passes precede a fixture environment failure in
`proc-bbbf8ed30d6d`. Missing configuration environment/header in that run and
`proc-292a7817bb88` were corrected for subsequent checks, not production failures
or passing whole chains. These are independent executions of authored tests,
not independent authorship or physical hardware certification. Scope and
publication/installed-host/exhaustive hardware exclusions above remain intact.

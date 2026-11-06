# Original VGA producer recurrence — 2026-11-06 (§7/API/CLI successor)

Engine21/representation9 appends source producer history to original VGA draw
traversal, not a selected VL/VH bundle or second installer. `vga_text_history.h`
is an internal owner implementation: source channel shifts, tail nibble, width,
uniform fill pixel/period and validity. No native pointer, surface/capacity snapshot,
Prepared state, repaint, register replay or guest time. Engine20 archives do not
have this layout and must not be used as engine21 proof.

Original text wrapper installs the source tail nibble before ordinary packed32
or SMZX byte glyph/expanded packed32 producer; observes the source-channel byte
afterwards. After migration, destination channel shifts resolve that observation
without interpreting source byte positions as destination RGB. It keeps the
complete original scratch backing, not just this derived recurrence. Nontext
live dispatch/raw text writer invalidates recurrence. Blank/uniform fills preserve
native source byte pattern across widths; complete clear records uniform zero.
BIOS overlay updates the source nibble using the original blue ADD arithmetic
(not a blue shift or normalized RGB), then performs the original local pixel
write. Flip/polled XOR updates source history only at its actual byte coordinate.
No implicit producer or writer executes in import. Fields are read once at the
owning traversal; invalid channels/booleans/period/nibble/active width fail the
load/capture, with usual fresh-target failure disposal (not rollback).

No public command/API change. Common file/inline stopped restore and explicit
Run semantics unchanged. Live renderer/backend routes are not disabled to meet
checkpoint tests. This increment supplements the earlier whole-producer bounds,
applied VGA/DAC/Int10/aperture and Render acquired-output histories.

## Reproduction and evidence

`python3 tests/original-foundation/vga-text-history.py` compiles the actual helper
and extracted original save/load field traversal with ASan/UBSan. Assertions:
source/destination R/B channel swap, uniform byte/word fill and odd width,
source nibble preserved across codec, XOR hit/miss, overlay's source blue ADD,
all truncated field lengths and invalid validity. `vga-text-bounds.py` checks
actual producer guards separately. These do NOT constitute full guest SMZX or
BIOS image/pixel qualification, all external writer certification, nor a real
SDL channel-swapped emulator fixture.

`proc-e8ae0b19604b` && exit0 initial helper+codec sanitizer, both builds and four
combined fresh FILE/INLINE E2Es. `proc-577be4e55b46` completed both sanitizers and
both builds but failed nonexistent native-console.py path; later tests did not
run. Correct `proc-88abf3859888` && exit0 passed real native held/status/explicit
Run/SB-FIFO/root42/mismatch/quit disposal (`native-console-43wc8dr_`), actual
mid-DAC component/graphics aperture continuation (`dac-restore-kivunsvg`), and
four combined E2Es. Final complete-clear uniform hook then `proc-e3e6dcdf510c`
&& exit0: helper+codec sanitizer, both builds, four combined E2Es
`speaker-timer`, `shared-file`, `sb-fifo`, `new-shell-child` in
`e2e-milestone-iajy6luq`, hashes/diff. Each E2E captures stopped, restores in
fresh process held, verifies observation, explicitly continues and exits root42.

`proc-d0cdf0222247` && exit0 adds real guest
`fresh-text-producer.asm`/`text-producer-restore.py`: 80-column text/VRAM,
explicitly enabled SMZX attribute mode, live loop before held save, fresh held
restore then explicit ordinary-text/panning loop/VRAM check/root42. Both configs
set `enable supermegazeux tweakmode=true`. Earlier `proc-bf5ec8a1bf84` lacked
that enable, so its pass is ordinary text only, NOT SMZX qualification. Neither
is a pixel golden or channel-swapped SDL/BIOS fixture.

Final Lua SHA256 `323603e95cd9cb71256099478cfee550ab6883d667cfc61d910c7ef2d9969dd9`,
native `8a047e98d3d0b46ef11fdb92bbc4d8b3bbaf2f7f371f821d9090a3c502c39cc8`.
Same generated headers/toolchain/configuration as INT10/FileLPT handoff;
card work `text-history-{lua,native}.log` contains build output. Parent-arranged
independent affected assessment still required. Remaining finite foundation
disposition and broader active-device/real producer variations are not implied
complete by these bounded author checks.

# Applied VGA continuation disposition — 2026-11-06

Current owner work, not whole finite-ledger closure. Selected reference
74b5fa198e619686d506d152b0edcd1d52d5bd0f versus
029bd65c5dcc7cdc7f2e14e35278620d9707d969; original4b7ffec provenance only.

## Direct original representation versus selected reconstruction helpers

`POD_Save/Load_VGA_Draw` transports `VGA_Draw`, not a reconstruction from current
CRTC values. Consequently effective values already represented include
address_add/line_length/linear_mask/planar_mask, address/address_line/total,
lines_done/lines_total/vblank_skip, split_line/has_split, panning/bytes_skip/
byte_panning_shift, width/height/bpp/screen_ratio, delay.{framestart,vrstart,
vrend,hrstart,hrend,hblkstart,hblkend,vblkstart,vblkend,vdend,vtotal,hdend,htotal,
singleline_delay}, render_step/render_max, blinking/blink/cursor/font backing,
must_draw_again/must_complete_frame/vret_triggered and modeswitch/doublescan
latches. `SerializeVga` also transports config/internal, seq/attr/crtc/gfx/dac,
VRAM/Tandy backing and extension state. Local linear/font pointers and drawline
handler IDs relocate; unknown handlers/capacity are refused. TempLine extent is
bounded and its bytes retained. No blanket SetupDrawing follows installation.

Selected effective-addressing, geometry and scan-timing records encode the
historical inputs needed to reconstruct these values rather than saving them.
That is not itself a missing original-owner behavior: using the already-applied
owner values is simpler and retains pending-register/applied-history distinction.
Text columns use the original `cols*2`; CGA4/3270PC keeps
`blocks*address_line_total`. This reasoning is representation coverage, not proof
of every model branch or active scanout pixel correctness.

Two actual omitted owner globals were found by this comparison:
`hretrace_fx_avg` feeds the next line's offset even when CRTC values match, and
`is_vga_rendering_on_demand` is the last applied schedule policy, not necessarily
the current requested configuration. The filter also consumes
`vga_display_start_hretrace`, latched at display start and subsequently distinct
from `crtc.start_horizontal_retrace`. They now travel in the original draw owner
beside existing FontMask/background. No duplicate logical codec, new producer
execution or guest time advance. Engine16 identifies the changed layout.

`tests/original-foundation/vga-filter.py` extracts actual save/load statements and
actual next-address filter update. Three different finite filter histories,
changed CRTC/display-start relation, both applied demand flags, destination
poisoning and every short input pass ASan/UBSan. This is a focused author witness,
not independent or complete video hardware acceptance.

## Current evidence and remaining exact gaps

`proc-267c83373652`: actual filter witness, both configured builds, four combined
FILE/INLINE fresh held→explicit continuation/root42/disposal E2Es
`e2e-milestone-ky9m89n7`, four Epson clean/pending/source-noninterference modes
`printer-restore-6xcgiz2x`, native no-debug/no-Agent/no-Lua held/Run/SB-FIFO/root42/
mismatch/quit `native-console-r_f5tn60`, diff check PASS.

Final SHA256 Lua `5e848bb2c86889e7380f8c51035e3d81a128c987429b5299687aae6f4b0034f0`,
native `430bef7bb556950f3e2f36fc8822f99d4f3b2c3c7d83e78c07b3431404a5be57`.
Build/config/toolchain paths and hashes unchanged from EPSON-RESTORE-DISPOSITION
except binaries/engine; its engine15 hashes remain historical receipts.
Intermediate engine16 filter-only binaries from proc-5d3cffd789c6 predate the
additional display-start latch; not final identities.

`proc-94ed58824c22` passes real exclusive Interactive/Bridge reconnect/ownership
and persistent evaluation routes, RPC session/result/lifetime/TSR refusal routes,
and production Lua worker callback/run/save routes. Logs at card work root
`engine16-{exclusive,rpc,lua-runtime}.log`. This run precedes the final display-start
latch addition, not a final-binary all-input acceptance. Engine15 full default
fresh-root matrix PASS `proc-72a0ddc8c26d`, `engine15-matrix.log` likewise remains
prior-input evidence.

Applied DAC history omission is now repaired in the same original owner traversal:
`tmp_dac[3]`, deferred counter and independent narrow/wide applied RGB/validity/
low-depth histories are transported. `vga_applied_palette.h` is faithfully ported
from selected74b5; ordinary SendColor records at the existing application seam.
Load repacks those histories locally without `VGA_DAC_UpdateColorPalette`, Render
palette delivery or changing newer pending `rgb[]`. Production `vga-dac.py`
exercises real write/send/save/load bodies: first/second component then fresh
third component; distinct applied banks versus pending RGB; destination shift
change; no Render delivery; every truncated input under ASan/UBSan. Initial
extraction-name error proc-74291e2c9c1a and wrong same-packing expectation
proc-2a47cf65ca05 failed; corrected expectation compares destination-local narrow
packing, not stale source bits. Both were test failures, not passes.

Final engine17 (representation9) receipt `proc-2895c3cadfea`: both builds,
filter/DAC sanitizer witnesses, all four combined E2Es `e2e-milestone-5wusz620`,
four printer modes `printer-restore-wywx8lgj`, native consumer
`native-console-pgf5cexb`, diff check PASS. Final Lua SHA256
`09b97bdf4be921e2a2d1f4a63b6dc9e20c10aa822c5bdc5e7c535ca5a21f614d`, native
`6ea2d22be49fb55b530f245fa8fcd9702e48b09d2df8363bb5a1db43f0528c11`.
Engine16 hashes above remain prior-input evidence, not current binaries.

Do not infer all selected text recurrence/native-format histories,
write-visibility or active FileLPT resources from this struct census. Complete
finite disposition, useful actual guest DAC/scanout history variations, dated §7
reconciliation and independent affected assessment remain required. Full Epson
fidelity is explicitly superseded by owner fault disposition, not on this gap list.

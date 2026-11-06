# VGA applied scan state — original owner disposition (2026-11-06)

This is a bounded successor to the selected-port ledger, not whole-foundation acceptance. Reference selected `74b5fa198e619686d506d152b0edcd1d52d5bd0f`, base `029bd65c5dcc7cdc7f2e14e35278620d9707d969`; candidate production owner at `1ec04fc9c5356295799476c1aaf85500d707b4a1` (engine21/representation9).

## §7 / API / CLI behavior

No new API/CLI or alternate codec. Original `POD_Save_VGA_Draw` / `POD_Load_VGA_Draw` transports the applied `vga.draw` structure, then relocates linear backing, font tables and line handler to destination-local resources. It does **not** infer applied state from newer CRTC registers, invoke SetupDrawing or advance a raster/timer. Restore is stopped on an initialized-unused fresh process; explicit continuation remains required. Unsupported backing/handler/font identities fail. Whole native POD retains the existing justified ABI constraint; this is not a portable archive claim.

Selected helper recipes do not require duplicate records in this owner:

| Selected representation | Actual original owner / live consumers | Disposition |
| --- | --- | --- |
| `vga_scan_addressing.h` | `vga.draw.address_add`, `VGA_CheckScanLength`, text-columns entry | Already carried. Existing `scan-stride.py` compares actual entry against pinned pristine upstream branches, preserving CGA2/CGA4 3270 blocks*address_line_total rather than the selected omission. |
| `vga_line_layout.h` | applied `line_length`, independent of resize-sentinel `width` | Already in draw POD; no width-register reconstruction. |
| `vga_split_compare.h` | `split_line` after render_max division, S3 zero/interlace, unsigned blank subtraction | Already in draw POD, consumed by split/raster scheduling. Actual live arithmetic now compared to selected resolver. |
| `vga_row_extent.h` | `address_line_total` after controller/CRTC/integrated scale/MCGA/doublescan choices | Already in draw POD. No re-reading current maximum_scan_line on install. |
| `vga_scan_schedule.h` | `render_max` and `doublescan_effect` | Both applied values carried. Draw mode/lines_done/address_line are carried separately, not reselected from scaler on load. |
| `vga_panning_scale.h` | `byte_panning_shift`, `bytes_skip`, `panning`, applied address | Already carried. MCGA VGA branch exits before linear scale, retaining reset0; planar/linear/packed use4, PC98/text2. |
| `vga_write_visibility.h` | `draw_base_planar` / `draw_base_size` | Already carried. Default all-visible and EGA/VGA split-prefix/ceil rows/planar division are directly implemented. Existing SVGA bank-switch visibility limitation remains, not fixed or certified here. |
| scan timing / blank policy | `delay` members, `vblank_skip`, dimensions and selected line mode | Applied results carried, including blank-wrap clipping and schedule; no recomputation from raw timing on load. This source disposition is not an active retrace golden. |

Applied history **outside** that structure did need changes: hretrace filter/display-start/demand and DAC latches (`09b33520`), text producer recurrence (`e114df11`, `fc6a08f8`), Render acquired output (`7f8a8941`); see their dedicated correction documents. Copying recipes in addition to these applied owner values would duplicate the codec without adding continuation information. Cursor native color loads (`1ec04fc9`) separately correct ordinary alignment/alias semantics.

## Reproduction and exact applicability

```
python3 tests/original-foundation/vga-selected-arithmetic.py
python3 tests/original-foundation/scan-stride.py
```

Requires Git objects for pinned selected and pristine upstream references, C++14 compiler, ASan/UBSan and `SAIVAGE_CARD_WORK_ROOT`. The first test extracts actual `VGA_Update_SplitLineCompare` and visibility block from the owning production traversal and compares the pinned selected arithmetic. Cases: S3/non-S3, zero compare, interlace, render1/2, blank subtraction/underflow; split before/equal/after height, row1/2/16 ceiling, EGA/LIN4/VGA unit division, text units, non-EGA/PC98 fallback, unsigned overflow. The scan test compares every actual original switch branch with upstream, integrated stride and text-column overrides. Neither executes guest pixels nor claims all hardware certification.

`proc-ef2dcd3de270` exit0 passes both. Initial test generator `proc-d220761d6660` failed compilation because extracted headers lacked separating newlines; corrected in test only, then the whole chain rerun. No emulator source or build input changed in this disposition increment. Builds/real held restore and explicit continuation on the same production source remain `proc-ce1654bbcdce`: both builds, fresh font/bank variations and four combined FILE/INLINE E2Es (`e2e-milestone-pj_ljquk`). These author checks are not independent acceptance. Cursor doc pins final binaries/config/toolchain. Remaining pixel/BIOS overlay/XOR/format and composed hardware qualifications are not silently certified by arithmetic.

# Render continuation correction — 2026-11-06

This is an additional bounded implementation, **not completion of the remaining foundation brief**. The selected acquired-output / cache-hit / cache-miss witness is implemented at `RENDER_StartLineHandler`. `SerializeRender` now carries palette changes, frame skip history, logical phase, cache bytes and cache cursor offset, aspect/changed-row history, acquired output cursor and RGB canvas. Destination pointers, pitch and pixel packing are reconstructed, not imported. Import does not run guest work.

`VGA_SetupDrawing(0)` was still called after the complete VGA draw record had been installed. That call is a mode transition, resets `lines_done`, and schedules drawing. It is removed: the 52-entry handler relocation and original draw record already own those values. Render local resource reconstruction preserves the VGA TempLine allocation (and `must_complete_frame`) rather than freeing another owner's restored backing.

## Evidence and failure applicability

The first engine14 combined E2E failed at native disposal. GDB hardware watchpoint on `*(long*)Drives[2]` established the writer as `BituMove` from `Normal1x_32_32_R`: output progressed to row401 of the 400-row surface and overwrote a DOS drive vtable with repeated color `0xaa`. This was not evidence of a DOS drive close defect. GDB batch wrappers returned zero despite the inferior crash; their earlier apparent PASS is invalid disposal evidence. The later passing real binary run below is not such a wrapper.

- `tests/original-foundation/render-lazy-cursor.py`: actual handler acquired hit/hit/miss, aspect2/3, unacquired miss, refusal and disabled path under ASan/UBSan.
- `tests/original-foundation/render-consumption.py`: actual owner codec plus actual handler with a stub output owner, cache offset independent of inLine, acquired hit/hit/miss **after owner import**, all truncations, invalid geometry/fps/ratio. This is not SDL/fresh-process pixel certification.
- `proc-9721342cddb2`, `work:///cards/card-n-c-l-a/e2e-milestone-f5z3csxe`: all four real combined debugger and Lua FILE/INLINE fresh-save/held restore/explicit continuation/root42 disposal routes passed.
- `proc-ffd1f8e4156a`, `work:///cards/card-n-c-l-a/fresh-root-matrix/run-s7kh05ol`: full default bounded fresh matrix passed; complete output at its returned stdout URL.
- `proc-daf86d4764d9`, `work:///cards/card-n-c-l-a/native-console-x2r55jkx`: configured native no-debug/no-Agent/no-Lua held/Run/root42/disposal and mismatch/quit passed. Earlier attempt used the native binary as the debugger producer and failed; corrected invocation supplies Lua debugger as `DOSBOX_TEST_BINARY` and native consumer as `DOSBOX_TEST_NATIVE_BINARY`.
- Lua/native builds succeeded (GCC13.3, inherited generated configurations). Lua binary SHA256 `40bd17c0b7066e692238c9cf4661c829700686d193359a772b40fdc39c48e2cd`, native `434d829620284849afc9ffb0e77ee94f536826d054b72b1e10f8c111e7134739`.

Engine identity is now14; earlier engine13 evidence applies only to its original input. Intermediate engine14 archives preceding the cache-offset correction are development artifacts, not compatible releases.

## Remaining limitations

The new presentation record explicitly refuses complex/traditional caches, locked/blit/xbrz/postrender-aspect and non-software-surface paths. This is bounded truthful refusal, **not** qualification of all selected presentation modes. `render-sdl.py` now exercises the production Render codec, lazy handler and GFX_RenderHistory body with a real SDL dummy window surface in separate save/load processes: RGB canvas, acquired local cursor/pitch, aspect2/3 hit-hit-miss without reacquisition (proc-5b3e4edeb367). It uses extracted production owners and fixture setup, not a full emulator scan producer; it does not certify every SDL backend. Full internal Epson fidelity is superseded by the explicit owner decision documented in `EPSON-RESTORE-DISPOSITION.md`; real mid-ESC/page fresh restore now faults only interrupted jobs. Engine15 supersedes engine14 for that added owner record. Final finite whole-brief disposition and independent assessment remain outstanding. MIDI/exhaustive composed hardware/installed-host qualification remain with their previously assigned scope, not reassigned here.

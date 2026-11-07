# Host-only printer admission correction (2026-11-06)

Successor to the engine29 delivery at d69e0b40 and combined documentation
533a68fcb5bdcf9cc18aa07557369f134ffd1793 / tree
fc300777c981295a7c9502c726174ce386c0ed14. Addresses root review F1;
not a publication or independent acceptance receipt.

## Owning correction and proportionate audit

`src/misc/savestates.cpp::state_config_identity` excludes Epson docpath,
openwith/openerror, shellhide and device. Inspection of PRINTER_Init and the
output consumers shows these select output directories, helper launch/window
policy and Windows physical destination, not guest hardware. The same exclusion
applies when disabled. No second validator or new configuration catalogue.
Printoutput/multipage remain: these select the spool/backend operation model;
dpi/width/height/printdbcs/timeout remain raster/parser/timing policy. Fontpath
is deliberately retained, not treated as proof of consumed bytes. Existing SDL,
render presentation, capture/title and launch-policy exclusions remain. This is
a same-cause bounded audit, not certification of all requested properties.

`src/hardware/parport/printer.cpp` now records actual SFNT bytes through the
resolved FreeType face, including the ordinary system/embedded/TTF fallback.
At initialization the same resolver identifies the five effective font choices
without creating a CPrinter/page, output or timer. Ordinary updateFont records
its actual consumed face under the same effective role (OCR-B shares OCR-A,
other non-special selectors use Roman). A missing face has an explicit empty
identity. A non-SFNT face makes checkpoint admission unavailable, without
breaking ordinary printing. No guessed pathname reopen certifies library input;
future external font mutation is not host-content rollback or pinned lazy IO.
Full interrupted-job reconstruction is still not implemented: ONLY unfinished
jobs fault the fresh destination; source remains healthy, no replay; idle/clean/
completed printers usable. Parent has legitimately conveyed the existing owner
notices for this scope. Root F2 authority acceptance remains upstream's decision.

## Actual affected checks and reproduction

Use existing configured `$W/build-{lua,native,debug-no-lua}` with GCC13.3,
NASM2.16.01, SDL2 2.30.0, FreeType pkg-config26.1.20. Config/generated-header
identities are unchanged from the final disposition/status4. Build all three
with `make -C "$W/build-<profile>" -j4`.

`DOSBOX_TEST_BINARY=$W/build-lua/src/dosbox-x python3
 tests/original-foundation/printer-policy.py` (repeat debug-no-lua).
Fixtures derive deterministic SFNT from tracked DOSBoxTTF.h and NASM
fresh-printer.asm; script and producer/consumer sources are versioned with this
change. Different directories/viewer/error helper/device/shellhide policy pass
for disabled, idle, page-pending, ESC-pending and completed-page cases. Every
case compares source files, archive SHA and stopped CPU/RAM/PIC/timer saves,
then explicitly continues to root42. Epson scheduled-timeout discard remains
the documented source/destination PIC exception, not elapsed guest time.
DPI180 rejects before evaluator; changing a valid SFNT name-table byte at the
same configured path also rejects. Idle/completed output appears only at the
new destination; unfinished destination produces no page and guest direct/BIOS/
DOS fault checks pass. Producer explicitly completes printing, proving healthy
source after capture. No physical printer/general DBCS certification implied.

Final `proc-ffbfb8e4aea9` exit0: all three builds; both five-case policy suites
(`printer-policy-k28v1r21`, `printer-policy-oixjojka`); all four composed debugger
→fresh FILE/INLINE held/save/delay/save→explicit Run/root42 cases
(`e2e-milestone-n0qbgu7d`). `proc-150ba78e4934` exit0: native console held/status/
delay→Run/SB FIFO/root42, wrong-slot/quit disposal/archive unchanged
(`native-console-y5seihx5`); real native DOS printer ESC/font/output cases for
empty/missing/valid/invalid/TTF resources (`printer-runtime-gw3glg3w`); actual TTF
face factory ASan/UBSan lifetime; diff check, toolchain and hashes.

Final SHA256 binaries:
* Lua: 7ada64fde468b74387aecabe643afb025473c02af881c6fbae862ea66a61d83a
* native: 1e7f4b14d3aed415f8a783cd17a0b9c42950bfd6f579395822631c6ddab75e3a
* debug-no-Lua: 48de59b5a58bc5d6783342ee88a6ec233088fd56cbd6801a0c470f3469511794

Failed exploratory fixture runs retained: proc-5516d60c6f37 builds passed but
producer looked for PS in its old directory after deliberately moving output;
fixed output-dir witness, not production behavior. proc-4a0c5c49f2f3 passed
policy restore/device negative but failed the assumed ASCII font marker;
replaced with actual SFNT name-table offset. Earlier d41f pass preceded role
canonicalization; final suites above rerun on final source. Missing exploratory
file paths and initial absolute apply_patch failures are not check passes.
Unchanged settled owner/regression evidence in REMAINING-FOUNDATION-DISPOSITION
and accepted history remains applicable only to unchanged inputs; all47 is not
claimed rerun here. No cleanup/remerge/service/publication/source switch.

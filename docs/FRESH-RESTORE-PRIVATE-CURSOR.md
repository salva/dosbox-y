# DOS private allocator cursor/native binding — 2026-11-05

## Owning safety gap

`DOS_GetMemory` in src/dos/dos_tables.cpp returns `dos_memseg`, appends
role/location/span to DOS_GetMemLog, then increments the cursor. It checks only
arena exhaustion, not overlap with previous allocations. DOS_TableBindings
already compares the original allocation log against freshly constructed native
owners. But POD_Load_DOS_Tables accepted *any* cursor within the private arena.
Thus valid-CRC Dos payload with the same native allocation records and cursor
minus one could cause a later original DOS_GetMemory call to overwrite a
retained allocation; plus one silently skips native storage. This was a real
consumed-state inconsistency, not a request for whole-RAM equality.

The owning load now requires the saved cursor equal the initialized cursor,
retaining the existing arena/unmapped/country casemap checks. Same allocation
log means same next-free location. Capture/log format is unchanged. Unsupported
unrepresented lazy allocations remain unsupported, rather than being made
apparently restorable by accepting a different cursor. No global registry,
parallel allocation engine, guest replay, rollback, IVT/default-vector equality
or public frontend restriction is introduced. Legitimate guest IVT hooks remain
payload. Other DOS private contents remain mutable guest state.

`DOS_GetMemory_reset`, DOS_FreeTableMemory and DOS_GetMemory_unmap are explicit
native lifecycle operations; restore is to the represented fresh startup, not
an arbitrary lifecycle replay. The check does not attempt to repair a mismatched
allocation history. Existing count/role/location/span mismatches still fail.

## Actual exact-build evidence

proc-1079f41992a6 exit0 sequential separately configured Lua/no-debug builds,
then real native SB held/status/delay/explicit Run/root42 positive and native
binding/quit negatives (`native-console-pm1njqz9`), new valid-CRC cursor +/-1
negatives (`private-cursor-w4mw9rca`), four combined debugger/FILE/INLINE workloads
(`e2e-milestone-0xzgv5hr`: speaker/PIT, shared files, SB FIFO, newly EXECed shell
child). All succeed; positive restore remains stopped until explicit Run.

proc-7d3a326a6cf5 uses that *new* producer archive and repeats cursor negatives
(`private-cursor-0rd_j8wp`). GDB at actual native POD_Load_DOS_Tables entry shows
fresh cursor50180, arena[50176,52224]; ZIP saved cursor50180. Therefore both
50179/50181 are demonstrably in the arena and would pass the old range check.
`build/private-arena-layout.log` retains this bounded observation. GDB then
explicitly killed its own disposable inferior, without running guest/console.
The first negative run used prior1003ee's producer archive, valid same wire
representation; the repeat proves exact current producer as well.

`tests/original-foundation/fresh-private-cursor.py` mutates only the final
uint16 Dos cursor (DOS_Tables is final subcodec, cursor its final field),
repacking valid CRC. Both Lua and native targets exit1 at Dos owner before
Lua evaluation/console publication/Run/root exit, with source and malformed
archives unchanged. This occurs after Memory and prior DOS subowners install;
no unchanged-destination/rollback assertion is made. Existing whole-process
fresh failure disposal applies. No exhaustive lazy allocator or device claim.

Evidence root `/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/`.
Build logs `build/private-cursor-{lua,native}.log`.
Lua binary `build/lua/src/dosbox-x` SHA256
`3fb40f34c32799528121923e0eb6a3d557b50e383b1301149f42d6a593d22586`;
native `build/no-debug/src/dosbox-x`
`86ee9efd6039422cbb051a45a4fdb811af48b5e977c9a38f2979a5f6ec415bd6`.
Profiles/config.h/config.status unchanged from1003ee, rehashed
proc-115839207ddf. No-Lua heavy-debug not rebuilt.

Reproduce: configure the established Lua and no-debug profiles; make sequentially;
set DOSBOX_TEST_BINARY and DOSBOX_TEST_NATIVE_BINARY and run
fresh-native-console.py; use its producer.log first line + `/first.sav` as
DOSBOX_TEST_ARCHIVE for fresh-private-cursor.py; run fresh-e2e-milestone.py with
Lua binary. SAIVAGE_CARD_WORK_ROOT owns disposable fixtures/logs.

This is author verification of one coherent native binding fix, not independent
approval or full accepted brief completion. ROM/HMA/UMB/MSCDEX/private extents
beyond this next-free binding, transitive constructor/termination effects,
close/timing/§7 and wider hardware obligations remain. The prior1003ee finite
ingress correction is settled per parent direction; this does not reopen it.

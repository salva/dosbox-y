# RTC continuation correction — 2026-11-06

## Selected versus original owner

Selected74b5fa198 `src/hardware/cmos.cpp`/`CMOS_LogicalRecord` includes
calendar, alarm, binary/BCD, 12/24-hour and SET modes, native `time_t`, and the
pending BIOS sync handoff. The inherited original `SerializeCmos` transported
only register bytes, selection/NMI, periodic rate/delay/ACK and last-ended.
Registers are not the authoritative live calendar: `cmos_readreg` consumes
`cmos.clock` and mode flags. A fresh process therefore substituted its startup
clock even when the captured guest had explicitly set and locked its RTC.

The correction appends those missing fields to the **existing original
SerializeGlobalPOD owner registration**. No duplicate record codec, Prepared,
preflight, host-time sampling, register write or timer advance is introduced.
PIC continues to own scheduled events. Engine22 deliberately rejects engine21
archives; representation9 and generated configuration remain unchanged. Native
time_t remains under existing same-ABI compatibility, not a portable format.
Ordinary host-sync policy is unchanged: future explicit guest ticks may sample
host time if enabled; held import does not. Full malformed CMOS/calendar value
admission is not established by this increment.

## Evidence and reproducible active witness

`proc-48f9c4bc88a6` passes `cmos-calendar.py`: actual owner registration with a
minimal POD harness plus actual `cmos_tick`, ASan/UBSan, calendar/alarm/modes/
pending-sync roundtrip, explicit leap day and month rollover. It also builds
both configurations (`$W/cmos-{lua,native}.log`). SHA256:

* Lua/heavy-debug/Agent: `ca4729e568870fbbbd9b7690912016164676ee9d404a43d0da426356abf1a51d`
* native/no-debug/no-Agent/no-Lua: `e7786723a6ea1e21ebc8d132f54776728a6a8b8d2ffec05a33dde47ae5b1f03c`

Compiler/config/header pins are unchanged from REMAINING-FOUNDATION-HANDOFF.
`$W=/work/dosbox-y/.saivage/work/cards/card-n-c-l-a`.
`fresh-cmos.asm` writes binary 24-hour SET-locked 2024-02-29 23:58:57, weekday4,
alarm seconds17/minutes18, NMI selection89h. The producer stops at0814:0200;
fresh continuation checks every programmed value before exit42. No destination
setup replay occurs. BIOS centuries/full alarm IRQ and free-running physical
RTC accuracy are not claimed.

```
export DOSBOX_TEST_BINARY=$W/build-lua/src/dosbox-x
export DOSBOX_TEST_NATIVE_BINARY=$W/build-native/src/dosbox-x
DOSBOX_TEST_CONSOLE_FIXTURE=cmos python3 tests/original-foundation/fresh-native-console.py
python3 tests/original-foundation/fresh-e2e-milestone.py
```

`proc-a08344736beb` passes native active-RTC held/status/host-delay invariant,
explicit Run/root42, wrong Shell-slot and quit disposal (native-console-0fcguumf),
four combined debugger→fresh FILE/INLINE held→explicit continuation E2Es
(e2e-milestone-8vokxajz), exclusive runtime and full-library production Lua.
`proc-11d4ae10d528` independently exercises debugger held/resaves/delay then
explicit Run/root42 using run-8djyrezt/first.sav. These are author checks, not
independent assessor acceptance. Earlier engine21 evidence stays applicable to
unchanged computations, not engine22 archive interchangeability.

Retained fixture failures: proc-9b8b0dac832f requested the wrong capture IP after
producer omitted CAPTURE_IP; proc-5db2bf7d917b captured correctly but expected
default0101 instead of0201 in its refusal-step assertion. proc-2a886c43e3d4
corrects both. proc-975129eb7442 passes held/step only: unrecognized TERMINATE
option did not request root exit; proc-11d4ae10d528 uses actual ROOT_EXIT and
checks root42. No production failure is concealed by these corrected invocations.

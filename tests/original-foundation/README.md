# Original foundation author probes

These candidate probes are **not** foundation acceptance. The pristine baseline
is selected upstream130de209abd6e6fb55b5535f85e2af4f6086ed58; candidate production
now begins shared VGA arithmetic and original PIC/GUS callback seams, not the
complete debugger/save port.

## Ordinary upstream baseline

Use a separate upstream build and a new disposable directory on each invocation:

```sh
python3 tests/original-foundation/upstream-slot.py \
  "$SAIVAGE_CARD_WORK_ROOT/upstream-baseline/src/dosbox-x" \
  "$SAIVAGE_CARD_WORK_ROOT/ordinary-slot-example" --memory 32 --cycles 6000
```

Requires nasm, Xvfb, libX11 and Python3. The helper allocates its own X server;
it never uses inherited display106. It sends events only to a uniquely matched
DOSBox window, runs the fixture, checks live counter/tick/input records, loads an
ordinary slot, closes normally, then starts another emulator and loads a retained
first-process slot with autosave disabled. The helper waits on its owned processes
and retains logs/receipt/config/fixture/archive in the supplied directory. It uses
SDL dummy audio: PIT register variation is **not audible output qualification**.
A missing load acknowledgement or guest-input observation is failure. Slot ZIP
CRC is reported, not a semantic state-equality assertion. Slot snapshots in the
receipt precede the load and may subsequently be overwritten by autosave; the
retained.sav is copied separately for the second run. Config changes between runs
are recorded by per-run config hashes.

Observed upstream baseline with binary SHA256
`327cc1d299bf2fa0e3ed7f51599d1c6dde1bf14401fe6f0cd22ab3c61844308d`:

- 32MiB/6000 same/fresh process load acknowledgement and normal exit:
  `work:///cards/card-n-c-a/upstream-slot-fresh-load/receipt.json`.
- 48MiB/9000 same/fresh process load acknowledgement and normal exit:
  `work:///cards/card-n-c-a/upstream-slot-48mb-9000/receipt.json`.
- 32MiB/6000 plus actual injected key record:
  `work:///cards/card-n-c-a/upstream-slot-input/receipt.json`.
  Runs report respectively24576/5461 distinct counter values and156/36 distinct
  BIOS tick values, key observed, load acknowledged and exit0.

These are deliberately bounded witnesses, not restored-register/RAM equality,
no-progress stopped inspection, XMS/EMS allocation, broad DOS-program or native
resource/lifetime coverage. Fresh process starts the fixture before loading; it
is not a fresh STOPPED debugger bootstrap. The fixture creates LIVE.BIN at startup:
its earlier file content is **not** a restoration oracle. Successful load logs do
not establish that all internal state or file cursors were restored correctly.
FluidSynth missing SoundFont diagnostics remain; MIDI is unqualified. Initial
F12+q close attempts timed out; WM_DELETE_WINDOW is the tested close approach.

## VGA 3270 stride regression

```sh
python3 tests/original-foundation/scan-stride.py
```

Compiles and executes actual `VGA_CheckScanLength` and its shared
`vga_scan_addressing.h` primitive from the candidate with small model globals.
Compares every enumerated mode/model/card and address shift0–2 for both architecture
branches with the pinned pristine-upstream function, then explicitly checks CGA2 and CGA4 on MCH_3270PC with two row totals and
ordinary CGA/EGA/MCGA/integrated cases. The shared resolver retains the upstream product; future reconstruction callers
must pass actual address_line_total. This is an arithmetic
regression, not a rendered-screen or whole owner reconstruction test.

## GUS/PIC and SB preservation seam

```
python3 tests/original-foundation/gus-pic.py
```

Compiles the actual GUS Tx write case/callback, PIC event/timer tables/index
accessors, original SerializePic getBytes/setBytes, and SB forward/read primitives
with inert other device targets and a small queue/global fixture. Exercises0.32ms
and value admission, callback-index roundtrip, loopback/output branches,
actual-port forwarding, serialized event value/time, queue relocation and ticker
list reconstruction/disposal. ASan/UBSan are enabled. Invalid event/timer indices
fail through the actual fatal boundary rather than out-of-bounds table access.
The production loader also rejects truncated/invalid queue links and heads;
these partial-install failures are fatal, not pre-effect refusals or rollback.
Original PIC table appends GUS_MidiTxDone without shifting prior indices.
This is not live full-machine archive reconstruction, real MIDI/audio or native
resource/lifetime qualification. Initial generator failures (NULL/duplicate
stand-ins, unsigned fixture constant unlike production's integer macro) were
corrected without production changes to accommodate them.

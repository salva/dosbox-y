# Original foundation author probes

These candidate probes are **not** foundation acceptance. The pristine baseline
is selected upstream130de209abd6e6fb55b5535f85e2af4f6086ed58; candidate production
now begins shared VGA arithmetic and original PIC/GUS callback seams, not the
complete debugger/save port.

## Shared native dispatch restoration

`python3 tests/original-foundation/dispatch.py` compiles the actual shared
`DOSBOX_DispatchDecoder`/`DOSBOX_DispatchCallback` function bodies with deterministic
native doubles. Both normal-loop and debugger callers now use these same
primitives. The fixture checks decoder and callback normal return, exceptions,
callback identity/flag restoration, and invalid callback indices before effect.
This preserves incoming page-fault policy and callback identity across unwind;
it does not itself establish frontier/save admission, core/flags borrowing, or
session disposal. Real caller/build/runtime checks remain separate.

The machine-service coordinator is now linked at `DOSBOX_RunMachine`: requests
remain data-only until a zero handler return, with no service on native return
or exception. Root, natural shell EXEC, callback/idle, fault and disk invocations
have explicit kinds; unclassified calls still deny service. Shell EXEC uses a
named wrapper rather than inferring authority from INT21 or register values.
`dispatch.py` also exercises the actual coordinator wrappers, zero/nonzero seam,
request retention under a callback ancestor and expiry on invocation return.
This does not yet register a frontend evaluator, bind native frontiers, settle
execution on owner departure, or admit any save operation. Classification alone
is neither execution nor save authority.

## Native primitives and worker (not yet emulator-integrated)

Run `python3 tests/original-foundation/native-primitives.py` with g++, pthread,
pkg-config and Lua 5.4. It compiles actual selected non-save owners and worker
with `-Wall -Wextra -Werror`; outputs live only under
`$SAIVAGE_CARD_WORK_ROOT/native-primitives`. Headers/worker and existing fixtures
are preserved from74b5fa198e619686d506d152b0edcd1d52d5bd0f, without codec families.
New `tests/machine-service/execution.cpp` covers exclusive attachment, stale
context, Pending/Active/Terminal, retained result, attachment/owner departure,
unlimited RUN and finite step budgets. Selected fixtures cover call-local
frontier authority, classified service, stop arbitration, worker file/code,
full libraries/require/observations/finalizers and cooperative close.
These passing primitive/transport checks do not establish machine admission:
CLI selection, actual frontend integration, semantic save joins and real native
resource/lifetime qualification are still unfinished. Worker is not yet linked
into the emulator. No complete usable debugger handoff is claimed.

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

## Corrected pristine baseline

Detached source130de209/tree6639975e876405ede1eee60dfb79fe5c7d23d63a was verified against all7829 tracked Git blobs with zero mismatches (`work:///cards/card-n-c-a/pristine-source-verification.json`). Additional generated autotools files are not claimed tracked upstream. Fresh `pristine-final-build` configure completed before make, using separate bare metadata pinned130de209 (configure reports130de20), flags `--enable-debug=heavy --enable-sdl2 --disable-opengl --disable-avcodec`. Build `proc-60ecbc06b09e` exit0; warnings remain. SHA256 identities:

- binary `aa145d4df9bd04e44f0fcd7413d13c2be1fe222bf393e4766bf7d981e18fd495`
- config.h `b737cfd5f99b83b337403aae3f1609e655abb3fcdb310d248860e60ea1eb016e`
- config.status `c2c04867b6e08ec80032a2d86a88268071d32cc525b332ec12927277c9cd9204`
- fixture `ae107bfe1dc63f7d0fc2bbbc7933100e5de574cd4fd50fabc52fa2cbc12a25ec`

Use `pristine-final-build/src/dosbox-x` instead of the older example binary above. Corrected runtime receipts supersede older build evidence without erasing its metadata/overlap limitations:

- `work:///cards/card-n-c-a/pristine-final-slots-32/receipt.json`,32MiB/6000, same/fresh counters25258/5461 and BIOS ticks161/36 (`proc-3bbbd36ad405` exit0).
- `work:///cards/card-n-c-a/pristine-final-slots-48/receipt.json`,48MiB/9000, counters36864/8192 and ticks157/36 (`proc-4c1949ec591e` exit0).

Each records exact per-run config hashes, two normal exits, input/load acknowledgements and five CRC-valid ZIP observations. All semantic and resource qualifications above still apply; this is not fresh STOPPED debugger restoration.

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

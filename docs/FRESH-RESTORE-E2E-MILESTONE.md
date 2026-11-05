# Fresh restore: finite first E2E assessment

This is an author handoff, not independent acceptance. Owner direction is
E2E-first, then successive corrections on this implementation. Broad corruption
or peripheral matrices are not prerequisites to this first bounded milestone.

## Exact candidate and entry points

Checkout `/work/dosbox-y/original-savestate-restart`, branch
`original-savestate-restart`. Production commit
`80406462c3f4b214d46ba2091d04311ff5897a06`; documentation successor010029187.
`SaveState::restoreFreshStartup(path)` in `src/misc/savestates.cpp` owns the
one-attempt disposable startup import. CLI: `-restore-state ARCHIVE`, optionally
`-lua-driver-file SCRIPT` or `-lua-driver-code CODE`. Without Lua, the supported
heavy-debug interface holds restored state for explicit debugger continuation.
Lua is a fresh interpreter: no Lua stack/interpreter/checkpoint is restored.
Live loads explicitly require a fresh launch. Repeated stopped saves remain.

Configured author binaries are under the card work directory
`/work/dosbox-y/.saivage/work/cards/card-n-c-k-a/build/{lua,no-agent,no-lua}/src/dosbox-x`.
Exact source/generated headers/configuration/toolchain/fixture receipts:
`build/sb-bounds-33-all3-manifest.json` SHA256
`d65e0ea6ce29d1ec493854103c68822bfa67420330f017efe0ae4ea2fb43b52c`;
`build/sb-bounds-auto-33-all3-manifest.json` SHA256
`36899f763ebc0942d51eff6fbb7631a364a0d55bae60c5e5e31be9161a83880f`.
These pin inputs; terminal test results separately establish author success.

## Finite independent assessment request

Parent [[card:card-n-c-k]] coordinates a non-author assessor. No assessor is
spawned or acceptance asserted by this document. Please assess this bounded
candidate now rather than waiting for all staged breadth. Use a separate owned
work directory, preserve original archives, and report exact candidate/binary,
commands, observed behavior, counterexample and correction scope. No service
reset, deployment, source switch or unknown-resource cleanup is needed.

1. Build/configure matching guest hardware and mounts, Lua/heavy-debug first.
   Run `tests/original-foundation/fresh-e2e-milestone.py` with
   `DOSBOX_TEST_BINARY` and `SAIVAGE_CARD_WORK_ROOT` pointing to your inputs/work.
   Dependencies include nasm, Xvfb, GDB, Lua5.4 development files, Python pexpect
   and ordinary configured emulator dependencies. Read each child log, not just
   the final wrapper line. Independently vary one workload or control sequence.
2. Speaker workload programs PIT2/gate before capture; after explicit continue
   it waits for two BIOS ticks, disables speaker and exits42. Shared-file
   workload saves duplicated handles at cursor3, observes current changed host
   contents, shares cursor through aliases and survives closing one alias;
   it exits42 only after these checks. SB workload consumes the saved unread
   reset response and executes E1 at actual port22c before exit42. Fourth
   descendant workload is captured at the initial stopped debugger entry;
   explicit continuation EXECs native `Z:\\COMMAND.COM /C EXIT`, receives child
   status and returns to the immutable root before root42. This covers legitimate
   new descendants through real FILE/INLINE as well as debugger, not shell-root
   capture.
3. For each, the real debugger saves the running workload. Fresh CLI restore
   holds before any requested debugger/Lua work; save/delay/save verifies no
   CPU/PIC/TIMER advance/rebudget. FILE/INLINE use new Lua states with full
   libraries, C require and finalizers. Explicit debugger and Lua Run then
   require exact root42, revoked context, Lua close and process disposal.
4. Try one malformed archive and actual source-close failure using existing
   `fresh-startup-failure.py` / `fresh-startup-close-failure.py`: require failure,
   target disposal, unchanged input, no evaluator/ordinary continuation. Do not
   demand preservation of a used target. Do not replay an uncertain operation.

Author runs `proc-0ccc62a7cec2` and stronger same-invocation
observe→delay→save→explicit Run `proc-f377a09109a1` exit0, evidence
`e2e-milestone-ua7nsl_2` / `e2e-milestone-tgqoxjhj`, all three debugger/FILE/INLINE verticals pass. Fourth descendant expansion `proc-6fff3560c3a2` exit0,
`e2e-milestone-0fd4_xa9`, all four verticals pass. Initial expansion
`proc-144d5202fc59` failed because an unnecessary breakpoint/Run at the already
held initial IP let the ordinary root exit before capture; first three cases
passed, descendant did not. Corrected fixture captures the existing initial
stop without Run; production unchanged. This is not evidence of shell-root
support. This
wrapper consolidates existing real consumers; it is not a new decoder or test
injection into production. All emulator/Xvfb instances it creates are owned
and settled by those consumers.

## Supported bounds and successive iterations

Current usable bound: internal DOS application root, matching initialized
hardware/mounts, common revision9 owner codec, fresh initialized-unused target,
heavy-debug debugger or fresh Lua interface. New EXEC descendants may return;
restored invocation root termination ends the disposable session. All3 configured
Lua/no-Agent/no-Lua author profiles have meaningful root/child/TSR, files/search,
RAM/XMS/EMS/input/video/timer consumers. Real SDL/Xvfb disk-audio to dummy backend
positive retains emulated hardware/audio; not waveform/frame parity.

Do not infer support for external boot, nonempty devices, arbitrary lazy native
allocations, shell-root captures, MIDI, all ROM/device extents, all timing
algorithms or no-debug continuation. No-debug/debug callback-slot differences
are currently refused, not remapped or waived. Hardware identity beyond resolved
RAM/loaded immutable inputs still has overrestrictive requested-property hashing
and incomplete effective-owner coverage. General frontend close races and full
non-debug timed-autosave integration remain staged. Safe disposal/input integrity,
root lifetime/ownership and no implicit execution are not staged away.

Next iteration: take the non-author E2E findings first; repair concrete blockers
at their owning primitive, rerun this finite vertical plus affected regressions,
then expand workload/native-binding breadth. Parent retains wider composed
acceptance; this handoff neither changes approved requirements nor claims it.

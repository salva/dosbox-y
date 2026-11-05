# Initialized CPUID vendor increment — author affected handoff

Selected from actual CPU initialization and CPU_CPUID leaf-zero consumption,
not a catalogue sweep. `cpuid string` is truncated to twelve initialized bytes
and padded with spaces; empty uses GenuineIntel. Requested text equality wrongly
rejects both explicit default and harmless suffix differences. Added
CPU_StateCpuidVendor() in CPU owner, consumed by common Config_Identity in place
of only requested cpu/cpuid string. Presence/model FPU contribution and all other
conservative guards remain. No architecture/core equivalence assumption,
dynamic-state mismatch/rebudget, replay or parallel decoder. No wire layout
change; old config-hash archives reject, no migration.

`fresh-cpuid.asm` checks all three CPUID vendor registers and exits42 only on
GenuineIntel (otherwise99). `fresh-effective-cpuid.py` captures at initial held
application entry through real debugger, then default -> explicit GenuineIntel
and GenuineIntelIgnoredSuffix positives observe/save/host-delay/save, explicitly
step and Run to actual CPUID check/root42/disposal. AuthenticAMD is a genuine
different initialized vendor; mismatch rejects Config_Identity, exit1/disposable
failure, archive unchanged, requested INLINE marker absent. This is not an
all-CPU or universal vendor-instruction qualification. Existing cputype guard
remains; below-486 CPUID support is not generalized.

Reproduce for each configured Lua/no-Lua binary:
```
DOSBOX_TEST_BINARY=/absolute/profile/src/dosbox-x \
 python3 tests/original-foundation/fresh-effective-cpuid.py
```
Build proc-d660a9741fd4 exit0, `build/cpuid-effective-{lua,no-lua}.log` under
card-n-c-k-a work root. Tests/full four-workload Lua E2E/diff-check and exact
binary/config.h/config.status/Makefile receipts: proc-9a3b3d6f2b9e exit0.
Focused evidence `effective-cpuid-dzeycfvn` (Lua), `effective-cpuid-jfc7ni9a`
(no-Lua); full Lua `e2e-milestone-8uyndpmb`. Binary SHA256 Lua
b296e971b572fa28361a36016c9ace5bb3eaa836b53ee61287faa2c4b63fc1f0,
no-Lua4056cdf960e6d9ea620ba2ee3ad7f770f0ea0487e5e011d5089921308079db25.
Configured headers/status/Makefiles unchanged from aa1bbbee receipts. No-agent
not rebuilt or promoted. No failing check in this increment.

Parent affected independent assessment request: inspect initialized default/
truncation bytes vs exact leaf-zero consumer, real CPUID/root42 positives,
genuine vendor mismatch disposal/no evaluator/input integrity, and combined
E2E preservation. Previous k-b status2 independently approves finite aa1bbbee
FPU increment only (verified reused builds, not rebuild), not this delta.
Broader native extents, other effective hardware, no-debug, close races, ingress
and §7 reconciliation remain unfinished; no design/provider/Run gate added.

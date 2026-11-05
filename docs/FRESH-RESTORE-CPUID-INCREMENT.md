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

## Independent affected adjudication (card-n-c-k-b)

APPROVE bounded production 2c0c159d32cb1be094a62bca97bd8b00c237e97d,
tree a8dcc174c072a789d390e7b4053fac1dbbb4481a; not broader goal acceptance.
Reused and independently hashed the exact author Lua/no-Lua binaries above;
config.h/config.status/Makefile hashes remain those independently recorded for
aa1b in test-card accepted status v2. No independent rebuild claimed.

CPU initialization clears the sentinel, copies at most12 bytes and space-pads;
leaf-zero returns those bytes in EBX/EDX/ECX or the GenuineIntel default.
CPU_StateCpuidVendor matches that consumer, including default/truncation/padding.
Only requested cpuid-string contribution changes; FPU and other guards remain.

Independent proc-1052eafac0e8 exit0 ran focused CPUID on both exact profiles,
focused FPU on Lua, then complete four-workload Lua E2E. Test-card evidence:
effective-cpuid-mirdzu7d, effective-cpuid-3a27_gex, effective-fpu-bz1pqesd,
e2e-milestone-07glcwci. Native root42 and FILE/INLINE42 plus lexical/GC context
revocation independently checked in proc-000ee072c69e. Actual guest fixture
checks all three vendor registers, not just archive metadata.

Nonblocking test-evidence finding: fresh-effective-cpuid.py requests the unknown
switch -lua-driver-inline; supported inline selection is -lua-driver-code
(include/lua_driver_options.h). Its absent marker alone does not prove a real
evaluator was requested. Original owner should correct that fixture argument.
Assessor did not repair it: independent proc-636556db1d58 used the supported
CODE selector and established AuthenticAMD rejection/exit1/no evaluator/source
unchanged. Same command verified old-aa1 archive rejection without migration,
and varied positive control to another suffix, seven-second hold, live-load
refusal and zero exit-prefix steps; actual CPUID/root42 passed. Evidence under
test-card cpuid-independent-bdif95qc; full process captures in its work root.

No implementation defect found or repaired. Prior804/aa1 runtime evidence
retains exact-input applicability, not promoted wholesale. All newly owned
processes settled; historical unknown resources untouched. Wider native extents,
effective hardware, no-debug, close races and ingress/docs remain owner work.

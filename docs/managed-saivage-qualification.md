# Installed Saivage consumer qualification

Run `tests/machine-service/managed-session-saivage.mjs` with Node 24, an installed
Saivage `dist/` (default `/opt/saivage`, override `SAIVAGE_RELEASE`), Xvfb, cc,
Lua 5.4 development headers and Python3. It calls actual `McpManager`, production
`mcpToolBinders`, `invokeToolForLlm` and result settlement. No model is constructed
or called. A new project/config/definition store is created under the card work
root; no live configuration is read or changed. The stdio server is the installed
public lua-repl-mcp, not a native adapter or a test RPC implementation.

Reproduction for the qualified delivery:

```
DOSBOX_TEST_BINARY=/work/dosbox-y/.saivage/work/cards/card-i-d/build-lua/src/dosbox-x \
DOSBOX_SESSION_INSTALL=/work/dosbox-y/.saivage/work/cards/card-i-d/install \
node tests/machine-service/managed-session-saivage.mjs
```

Host 4ec98bc6bf65aa64910df126cb9089958a6c7527 (tree
ed37ed715044f661681d6b4e5fc07b4459315045), module
0b8d271911c17ce762246afa86b74ab20e999343 (tree
4a8e9b8d4976152460398740e8de75ad2fff19bb). Binary SHA256
73b4362180ad63a6e75b4c1a4eb8669c5ca8811aca30d73cc62d0b1fa94bc8eb.
See managed-lua-session.md and card-i-d accepted status1 for install/build/ABI.
Node was v24.16.0; `/opt/saivage` has no Git metadata, so the deployment-doc pin
15a1a01106db80940777e98e83fb99b98b33814b is reported provenance, not a verified
checkout identity. Observed compiled SHA256:

* mcp/mcp-manager.js: 8d19e84225938e8c9d51ae0e8016240d6700617d8f19254fe86a94c9005ce039
* tools/mcp-provider.js: bac02e77b18159ee02104126fc9078f3d6614db19e522e30be8436d0db6e7a1a
* tools/invocation.js: f9ec6cb6c9cd8f7b00b87488fe82a0a1337d56526abf9c6ac23098e56930462f
* runtime/process-runner.js: cdd083510cdde78a061c7d2964408f18e6264a7acfdfd5e325fb02e6c3a93103

Final behavioral run: proc-09e19b6acfb5, exit0, empty stderr. Complete output is
`work:///cards/card-i-e/processes/proc-09e19b6acfb5/stdout.log`; retained project
`/work/dosbox-y/.saivage/work/cards/card-i-e/saivage-consumer-Y6NZ2I`.
It covers discovery and bound calls for all five tools, default/custom libraries,
compiled C require, globals, JSON, once-only file effect then Lua error, busy
execute/wait/kill, CLOEXEC, context/registers/step/stale rejection, breakpoint
read callback, fresh restore before ready, repeated stopped saves and explicit
Run to root_exit42 with outstanding execute/wait. Late wait reports
result_unavailable, never replay. Across fresh processes Memory archive offsets
32..47 are legacy MemoryBlock phandlers/mhandles pointers (LP64); only these
are masked. All RAM and other CPU/Pic/Keyboard/IntTimer10 bytes compare equal.
Repeated saves within the restored process compare without masking.

Actual emulator lifecycle cases record PID and open FD count (9), then assert
/proc/PID absent after execution deadline, direct SIGKILL, idle manager stop,
active manager stop, observer kill, save-request manager stop and cancellation.
Manager stop is actual provider/transport destruction, not native Stop. Fresh
binding/catalog discovery follows re-start. Final cleanup joins the manager and
Xvfb. Save interruption proves disposal with accepted pending save; it does not
prove which internal save phase was reached. No archive existed at its final
path; this does not prove rollback or temporary-file cleanup. Observer kill
similarly proves pending observer-bearing work teardown, not synchronized entry
into its callback. No hard-kill Lua finalizer claim is made.

Earlier strengthened read-only mutation probes returned native reason `error`
after one callback; they are not successful breakpoint verdicts. The final
passing callback intentionally only reads, and asserts breakpoint. The failed
same-location registration probe (proc-98f1858d642f) is a test error, not evidence
of owner correction. Raw output remains retained. Parent assessment should not
conflate the successful read-only workload with exhaustive mutation-denial proof.
Other authored failures: missing owning invocation context; expecting success
for MCP isError; killing a deadline-removed session; comparing raw relocated
pointer bytes; an unbounded breakpoint workload on an exiting tiny program.
That run proc-0970fb8f4c00 was explicitly killed, not cleanly finalized. A later
process-table check found no consumer/Xvfb matching processes; that observation
does not settle unrelated historical unknown resources.

These are author consumer checks, not independent whole-priority acceptance.
No host production source, live service, credential, model route or licensing
was changed. Native adapter/archive author tests in card-i-d remain separate
applicable evidence, not Saivage substitutes. Full callback phase-synchronized
interruption and arbitrary foreign-C/fsync interruption are not claimed.

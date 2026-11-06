# PC speaker owner history correction — 2026-11-06

Selected74b5fa198 versus029bd65 `src/hardware/pcspeaker.cpp` moves the
AddDelayEntry previous-output edge filter and raw clock gate from two statics
into the speaker owner, initializes them on construction and nulls teardown
identities. Candidate43ab9b270 still omitted this ordinary/non-save behavior:
the original POD saved queued edges but not the filter deciding the next edge,
and PCSPEAKER_UpdateType consumed a fresh raw gate rather than captured history.

The direct owning fix moves both booleans into original `spkr`; existing
POD_Save/Load transports them with the queue and retains destination channel.
No second codec, setup replay, synthesized samples, timer advancement, Prepared
or rollback. The enlarged POD deliberately changes engine22→23; representation9
and APIs/CLI remain unchanged. Engine22 receipts are not engine23 compatibility
claims. Ordinary initialization now resets history, teardown clears owner/channel.

`tests/original-foundation/speaker-history.py` consumes the actual owner fields
and AddDelayEntry, checks captured-high→same-high(no edge)→low(one edge), raw
gate, and full queue history under ASan/UBSan. This is reduced producer evidence,
not rendered/acoustic comparison or malformed owner admission certification.
`proc-dd240e1619c8` && exit0 passes that test, all three configured builds and
diff check. Build logs in card work `speaker-history-{lua,native,no-lua}.log`.
Engine23 binary SHA256:
- Lua `e33d6646e29c8a6f47940300d52af04cc1809db8335630db0472556509c73c7a`
- native `ca1a7f9ab8f5866ceb9955502852078c1fdfc55c63899da814cd9b6a53bc1dce`
- debug-no-Lua `28cc3dc02db691f1df44dd26f522be653b840eef2f9dba9c3e036bb08460ce46`

Configuration/generated headers/toolchain unchanged from handoff profiles.
`proc-b4f4df6f3371` && exit0 passes real fresh speaker-timer/speaker-active/RTC,
all four combined debugger→fresh FILE/INLINE held observation→explicit Run/root42
disposal E2Es (`e2e-milestone-wuw8tx3h`) and actual no-Lua Interactive unavailable/
native steps. Not independent approval or whole finite foundation closure.
Two apply_patch calls returned no changes (absolute then project-relative path);
selected exact edits effected the fix. Exploratory wrong src/gui/savestates.cpp
grep failed, not a build failure; owner resides src/misc/savestates.cpp.

#!/usr/bin/env python3
"""Finite real-UI author milestone; not independent acceptance or broad coverage.

Requires configured Lua/heavy-debug DOSBOX_TEST_BINARY, nasm, Xvfb, gdb,
Lua5.4 development files and the normal stopped-save probe dependencies.
Each producer runs a real DOS workload before debugger capture. Restore uses
CLI into a new process; Lua is a new interpreter, never serialized.
"""
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
work = Path(tempfile.mkdtemp(prefix='e2e-milestone-',
                            dir=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])))
binary = os.environ['DOSBOX_TEST_BINARY']
env = {k: v for k, v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env.update(DOSBOX_TEST_BINARY=binary, DOSBOX_TEST_AUDIO='1')
print(work, flush=True)

def run(name, probe, extra):
    result = subprocess.run(['python3', str(here / probe)],
                            env=dict(env, **extra), text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (work / (name + '.log')).write_text(result.stdout)
    result.check_returncode()
    return result.stdout

cases = (
    ('speaker-timer', {'DOSBOX_TEST_SPEAKER': '1',
                       'DOSBOX_TEST_SPEAKER_ACTIVE': '1'}, '0814:0201'),
    ('shared-file', {'DOSBOX_TEST_SHARED_FILE': '1'}, '0814:0204'),
    ('sb-fifo', {'DOSBOX_TEST_SB_HELD': '1'}, '0814:0201'),
    ('new-shell-child', {'DOSBOX_TEST_EXEC_CHILD': '1',
                         'DOSBOX_TEST_CHILD_SHELL': '1',
                         'DOSBOX_TEST_CAPTURE_IP': ''}, '0814:0101'),
)
for name, setup, step in cases:
    producer_setup = {'DOSBOX_TEST_CAPTURE_IP': '0814:0200', **setup}
    producer_setup['DOSBOX_TEST_REFUSAL_REGISTERS'] = 'CS:IP=' + step
    output = run(name + '-producer', 'stopped-save.py', producer_setup)
    archive = str(Path(output.splitlines()[0]) / 'first.sav')
    run(name + '-debugger', 'fresh-startup.py', {
        'DOSBOX_TEST_ARCHIVE': archive, 'DOSBOX_TEST_ROOT_EXIT': '1',
        'DOSBOX_TEST_STEP_IP': step,
        'DOSBOX_TEST_ROOT_FACTS': 'code=42 resident=0 self-parent=0'})
    # Real FILE/INLINE observe/save/host-delay/save without advancing guest.
    run(name + '-lua-held', 'fresh-startup-lua.py', {'DOSBOX_TEST_ARCHIVE': archive})
    # Same fresh FILE/INLINE invocation observes/delays/saves and then explicitly
    # runs to useful guest result, exact terminal facts and Lua close fence.
    run(name + '-lua-run', 'fresh-startup-terminal.py', {
        'DOSBOX_TEST_ARCHIVE': archive, 'DOSBOX_TEST_OBSERVE_BEFORE_RUN': '1'})
    print(name, 'debugger + fresh FILE/INLINE held observation, explicit continuation, root42 disposal PASS', flush=True)
print('Bounded author E2E milestone PASS; independent assessment still required', flush=True)

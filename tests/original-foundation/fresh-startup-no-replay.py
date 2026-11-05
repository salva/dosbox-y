#!/usr/bin/env python3
"""Actual native-construction/install path must not dispatch guest work.

GDB observes entry through the startup hold; later attachment/delay/continuation
are covered separately by fresh-startup.py and fresh-startup-lua.py. This does
not prove every hardware/configuration constructor is guest-free.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'fresh-native-entry'
root.mkdir(exist_ok=True)
work = Path(tempfile.mkdtemp(prefix='no-replay-', dir=root))
archive = Path(os.environ['DOSBOX_TEST_ARCHIVE']).resolve()
before = hashlib.sha256(archive.read_bytes()).hexdigest()
conf = work / 'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n'
                '[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
# Start watching before native construction, not at the first restored owner.
# No guest decoder, callback guest re-entry, queue dispatch or timer tick may
# occur between this seam and the debugger startup hold.
script = work / 'guard.gdb'
script.write_text('''set pagination off
set confirm off
set breakpoint pending off
break VM_ConstructFreshNative
run
python
import gdb
assert gdb.newest_frame().name() == 'VM_ConstructFreshNative'
class Forbidden(gdb.Breakpoint):
    def stop(self):
        gdb.execute('bt')
        raise gdb.GdbError('unexpected guest dispatch before startup hold: '+self.location)
for name in ('DOSBOX_RunMachine', 'CPU_Core_Normal_Run', 'PIC_RunQueue', 'TIMER_AddTick',
             'CALLBACK_Idle', 'CALLBACK_IdleNoInts',
             'CALLBACK_RunRealInt', 'CALLBACK_RunRealIntOwned',
             'CALLBACK_RunRealInt_retcsip', 'CALLBACK_RunRealFar',
             'CALLBACK_RunRealFarInt', 'CALLBACK_RunShellExec'):
    Forbidden(name)
end
break DEBUG_HoldRestoredStartup
continue
python
assert gdb.newest_frame().name() == 'DEBUG_HoldRestoredStartup'
print('FRESH_NATIVE_NO_REPLAY_HOLD_REACHED')
end
quit
''')
result = subprocess.run(['gdb', '-q', '-batch', '-x', str(script), '--args',
                         os.environ['DOSBOX_TEST_BINARY'], '-conf', str(conf),
                         '-restore-state', str(archive)], capture_output=True,
                        timeout=60, env=dict(os.environ, SDL_VIDEODRIVER='dummy',
                                            SDL_AUDIODRIVER='dummy', TERM='xterm'))
(work / 'gdb.log').write_bytes(result.stdout + result.stderr)
assert result.returncode == 0, (work, result.returncode)
assert b'FRESH_NATIVE_NO_REPLAY_HOLD_REACHED' in result.stdout, work
assert b'Python Exception' not in result.stderr and b'Traceback' not in result.stderr, work
assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
print(work, 'native construction/install reached hold without guest dispatch/timer/queue')

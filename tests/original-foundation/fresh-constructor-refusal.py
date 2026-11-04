#!/usr/bin/env python3
"""Fresh native layout CPX dependency refuses, without attaching Lua or replay.
Uses a matching baseline fixture config plus a German layout requiring CPX.
This is explicit unsupported construction, not German-layout restore support.
"""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
w = Path(tempfile.mkdtemp(prefix='constructor-', dir=Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'fresh-callback-entry'))
archive = Path(os.environ['DOSBOX_TEST_ARCHIVE'])
before = hashlib.sha256(archive.read_bytes()).hexdigest()
config = w / 'fixture.conf'
config.write_text(Path(os.environ['DOSBOX_TEST_CONFIG']).read_text() + '\n[dos]\nkeyboardlayout=de\n')
marker = w / 'evaluator-entered'
args = [os.environ['DOSBOX_TEST_BINARY'], '-conf', str(config), '-restore-state', str(archive)]
if os.environ.get('DOSBOX_TEST_LUA'):
    args += ['-lua-driver-code', f"local f=assert(io.open('{marker}','w')); f:close()"]
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', TERM='xterm')
with (w / 'process.log').open('wb') as log:
    result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=30)
assert result.returncode == 1, result.returncode
assert not marker.exists()
assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
# Observe the actual refusal stack independently; exit1 alone is insufficient.
result = subprocess.run(['gdb', '-q', '-batch', '-ex', 'break E_Exit', '-ex', 'run', '-ex', 'bt', '--args', *args],
                        capture_output=True, text=True, env=env, timeout=30, check=True)
(w / 'refusal.gdb.log').write_text(result.stdout + result.stderr)
for expected in ('guest callback required', 'RequireGuestCallbackEntry',
                 'keyboard_layout::read_codepage_file', 'VM_ConstructFreshNative'):
    assert expected in result.stdout, result.stdout
print(w, 'CPX native construction refuses/exit1, no evaluator, unchanged source archive; actual callback-boundary stack')

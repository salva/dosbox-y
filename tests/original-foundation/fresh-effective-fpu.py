#!/usr/bin/env python3
"""Initialized FPU compatibility: real equal-owner positives and disposal negatives."""
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
work = Path(tempfile.mkdtemp(prefix='effective-fpu-', dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env = {k: v for k, v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env.update(DOSBOX_TEST_BINARY=os.environ['DOSBOX_TEST_BINARY'], DOSBOX_TEST_AUDIO='1')
print(work, flush=True)

def run(name, probe, extra):
    result = subprocess.run(['python3', str(here / probe)], env=dict(env, **extra),
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (work / (name + '.log')).write_text(result.stdout)
    result.check_returncode()
    return result.stdout

out = run('producer', 'stopped-save.py', {
    'DOSBOX_TEST_SB_HELD': '1', 'DOSBOX_TEST_CAPTURE_IP': '0814:0200',
    'DOSBOX_TEST_REFUSAL_REGISTERS': 'CS:IP=0814:0201', 'DOSBOX_TEST_FPU': 'true'})
archive = Path(out.splitlines()[0]) / 'first.sav'
before = hashlib.sha256(archive.read_bytes()).hexdigest()
for value in ('1', 'auto'):
    run('equal-' + value, 'fresh-startup.py', {
        'DOSBOX_TEST_ARCHIVE': str(archive), 'DOSBOX_TEST_FPU': value,
        'DOSBOX_TEST_ROOT_EXIT': '1', 'DOSBOX_TEST_STEP_IP': '0814:0201'})
for value in ('false', '387'):
    conf = work / (value + '.conf')
    conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\nfpu=' + value +
                    '\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
    marker = work / (value + '.evaluator')
    script = "local f=assert(io.open(" + repr(str(marker)) + ",'w')); f:write('ran'); f:close()"
    args = [env['DOSBOX_TEST_BINARY'], '-conf', str(conf), '-restore-state', str(archive)]
    if os.environ.get('DOSBOX_TEST_LUA_EVALUATOR'):
        args += ['-lua-driver-code', script]
    result = subprocess.run(args,
        env=dict(env, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy'),
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=40)
    (work / (value + '.log')).write_text(result.stdout)
    assert result.returncode == 1, result.stdout
    assert 'Config_Identity: incompatible or malformed' in result.stdout, result.stdout
    assert 'disposable target cannot continue' in result.stdout, result.stdout
    assert not marker.exists(), result.stdout
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
    if os.environ.get('DOSBOX_TEST_LUA_EVALUATOR'):
        conf.write_text(conf.read_text().replace('fpu=' + value, 'fpu=true'))
        control = subprocess.run(args,
            env=dict(env, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='dummy'),
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=40)
        (work / (value + '-control.log')).write_text(control.stdout)
        assert control.returncode == 0 and marker.read_text() == 'ran', control.stdout
assert hashlib.sha256(archive.read_bytes()).hexdigest() == before
print('FPU true -> 1/auto held/step/Run/root42 PASS; absent/387 mismatch exit1; source unchanged', flush=True)

#!/usr/bin/env python3
"""Real non-save owners and Lua worker; not emulator admission proof."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'native-primitives'
work.mkdir(exist_ok=True)
environment = os.environ.copy()
subprocess.run(['g++', '-std=gnu++14', '-Wall', '-Wextra', '-Werror',
                '-shared', '-fPIC',
                str(root / 'tests/machine-service/worker-native.cpp'),
                *subprocess.check_output(
                    ['pkg-config', '--cflags', '--libs', 'lua5.4'], text=True).split(),
                '-o', str(work / 'worker_native.so')], check=True)
environment['WORKER_CPATH'] = str(work / '?.so')
for name in ('check', 'frontier', 'execution', 'stop-conditions', 'handoff', 'options', 'worker'):
    command = ['g++', '-std=gnu++14', '-Wall', '-Wextra', '-Werror', '-pthread',
               '-I' + str(root / 'include'),
               str(root / 'tests/machine-service' / (name + '.cpp'))]
    if name == 'worker':
        command += [str(root / 'src/debug/lua_driver_worker.cpp')]
        command += subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'lua5.4'], text=True).split()
    command += ['-o', str(work / name)]
    subprocess.run(command, check=True)
    subprocess.run([str(work / name)], check=True, timeout=30,
                   cwd=work, env=environment)
    print(name + ' PASS (native primitive / worker transport only)', flush=True)

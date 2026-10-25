#!/usr/bin/env python3
"""Bounded pristine-upstream ordinary-slot probe; no immediate-stop claim."""
import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import time
import zipfile
p = argparse.ArgumentParser()
p.add_argument('binary', type=Path)
p.add_argument('work', type=Path)
p.add_argument('--memory', type=int, default=32)
p.add_argument('--cycles', type=int, default=6000)
p.add_argument('--gus', action='store_true', help='enable native GUS allocation/slot traversal; not active GUS playback proof')
p.add_argument('--second-sb', action='store_true', help='enable second native Sound Blaster; not DMA/playback qualification')
a = p.parse_args()
a.binary = a.binary.resolve()
a.work = a.work.resolve()
a.work.mkdir(parents=True, exist_ok=True)
source = Path(__file__).with_name('upstream-slot.asm')
subprocess.run(['nasm', '-f', 'bin', *(['-DGUS_DRAM'] if a.gus else []), str(source), '-o', str(a.work/'WITNESS.COM')], check=True)
config = a.work/'baseline.conf'
config.write_text(f'''[sdl]
output=surface
fullscreen=false
[dosbox]
memsize={a.memory}
quit warning=false
hostkey=ctrlalt
captures={a.work}/capture
[sdl]
mapperfile_sdl2={a.work}/mapper.map
[render]
frameskip=0
[cpu]
core=normal
cycles=fixed {a.cycles}
[sblaster2]
sbtype={'sb16' if a.second_sb else 'none'}
sbbase=240
irq=5
dma=3
hdma=6
oplmode=none
[gus]
gus={'true' if a.gus else 'false'}
[dos]
xms=true
ems=true
[dosbox]
saveremark=false
forceloadstate=true
autosave=3 1-2
[autoexec]
mount c {a.work}
c:
witness.com
''')
env = dict(os.environ, SDL_AUDIODRIVER='dummy')
# Xvfb allocates a new free display atomically, never touches inherited servers.
readfd, writefd = os.pipe()
xlog = (a.work/'xvfb.log').open('wb')
x = subprocess.Popen(['Xvfb', '-displayfd', str(writefd), '-screen', '0', '1024x768x24', '-nolisten', 'tcp'], pass_fds=(writefd,), stdout=xlog, stderr=subprocess.STDOUT)
os.close(writefd)
try:
    with os.fdopen(readfd) as display:
        number = display.readline().strip()
    if not number:
        raise RuntimeError('Xvfb failed to allocate display')
    env['DISPLAY'] = ':'+number
    receipts = []
    for index in range(2):
        if index == 1:
            config.write_text(config.read_text().replace('autosave=3 1-2', 'autosave=0'))
            shutil.copyfile(a.work/'retained.sav', a.work/'1.sav')
        log = (a.work/f'run{index}.log').open('wb')
        guest = subprocess.Popen([str(a.binary), '-conf', str(config), '-savedir', str(a.work)], cwd=a.work, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            time.sleep(10 if index == 0 else 2)
            key = Path(__file__).with_name('x11-key.py')
            subprocess.run(['python3', str(key), 'DOSBox', 'a'], env=env, check=True)
            time.sleep(0.5)
            if guest.poll() is not None:
                raise RuntimeError(f'guest exited early: {guest.returncode}')
            slots = []
            for slot in sorted(a.work.glob('*.sav')):
                raw = slot.read_bytes()
                with zipfile.ZipFile(slot) as z:
                    bad = z.testzip()
                    if bad is not None:
                        raise RuntimeError(f'bad slot CRC: {slot}: {bad}')
                    slots.append({'path':str(slot), 'sha256':hashlib.sha256(raw).hexdigest(), 'bytes':len(raw), 'bad_crc':bad, 'components':z.namelist()})
            live = (a.work/'LIVE.BIN').read_bytes()
            records = [live[i:i+6] for i in range(0, len(live)-5, 6)]
            receipts.append({'run':index, 'slots':slots, 'live_file_bytes':len(live),
                             'counter_values':len({r[:2] for r in records}),
                             'tick_values':len({r[2:4] for r in records}),
                             'injected_key_seen':any(r[4:6] == b'\x61\x1e' for r in records)})
            if not receipts[-1]['injected_key_seen']:
                raise RuntimeError('guest key variation not observed in live records')
            key = Path(__file__).with_name('x11-key.py')
            subprocess.run(['python3', str(key), 'DOSBox', 'Control_L', 'Alt_L', 'l'], env=env, check=True)
            time.sleep(2)
            receipts[-1]['loaded_log'] = 'Loaded. (Slot' in (a.work/f'run{index}.log').read_text(errors='replace')
            if not receipts[-1]['loaded_log']:
                raise RuntimeError('ordinary load acknowledgement absent')
            if index == 0:
                shutil.copyfile(a.work/'1.sav', a.work/'retained.sav')
            receipts[-1]['config_sha256'] = hashlib.sha256(config.read_bytes()).hexdigest()
            # Explicitly request normal emulator shutdown; forced cleanup is separate.
            subprocess.run(['python3', str(key), 'DOSBox', 'WM_DELETE_WINDOW'], env=env, check=True)
            guest.wait(timeout=10)
            receipts[-1]['exit']=guest.returncode
        finally:
            if guest.poll() is None:
                guest.terminate()
                try:
                    guest.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    guest.kill()
                    guest.wait()
            log.close()
    (a.work/'receipt.json').write_text(json.dumps({'binary':str(a.binary), 'binary_sha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(), 'config_sha256':hashlib.sha256(config.read_bytes()).hexdigest(), 'fixture_sha256':hashlib.sha256((a.work/'WITNESS.COM').read_bytes()).hexdigest(), 'runs':receipts}, indent=2))
    print(json.dumps(receipts, indent=2))
finally:
    x.terminate()
    x.wait(timeout=5)
    xlog.close()

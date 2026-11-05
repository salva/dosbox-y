#!/usr/bin/env python3
"""Reject in-arena DOS private allocator cursor drift, with valid ZIP CRC.
Use an actual stopped-save archive. DOS_Tables is the final Dos subcodec and
its uint16 next-free cursor is the final field; no ABI-sized offsets guessed.
Positive workload and held/Run behavior are checked by fresh-native-console.py.
"""
import hashlib, os, pathlib, subprocess, tempfile, zipfile
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='private-cursor-',dir=root))
print(w,flush=True)
source=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
original=hashlib.sha256(source.read_bytes()).hexdigest()
conf=w/'fresh.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
with zipfile.ZipFile(source) as z:
 cursor=int.from_bytes(z.read('Dos')[-2:],'little')
 assert 1 < cursor < 65534,cursor
# Both remain in the existing private arena for the default native workload:
# minus one overlaps the last retained allocation; plus one skips native space.
# The old range-only consumer accepted both despite identical allocation log.
for delta in (-1,1):
 bad=w/f'cursor-{delta:+d}.sav'
 with zipfile.ZipFile(source) as z,zipfile.ZipFile(bad,'w') as out:
  for item in z.infolist():
   data=z.read(item.filename)
   if item.filename=='Dos': data=data[:-2]+(cursor+delta).to_bytes(2,'little')
   out.writestr(item,data)
 digest=hashlib.sha256(bad.read_bytes()).hexdigest()
 for profile in ('LUA','NATIVE'):
  binary=os.environ['DOSBOX_TEST_BINARY' if profile=='LUA' else 'DOSBOX_TEST_NATIVE_BINARY']
  args=[binary,'-conf',str(conf),'-restore-state',str(bad)]
  args+=['-lua-driver-code','error("CURSOR_EVALUATOR_MUST_NOT_RUN")'] if profile=='LUA' else ['-restore-console']
  r=subprocess.run(args,input='run\n',text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45,
      env=dict(os.environ,TERM='xterm',SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
  (w/f'{profile}-{delta:+d}.log').write_text(r.stdout)
  assert r.returncode==1 and 'State component Dos failed during installation' in r.stdout,r.stdout
  for forbidden in ('Restored console held:','Restored session ended','CURSOR_EVALUATOR_MUST_NOT_RUN'):
   assert forbidden not in r.stdout,(forbidden,r.stdout)
  assert hashlib.sha256(bad.read_bytes()).hexdigest()==digest
assert hashlib.sha256(source.read_bytes()).hexdigest()==original
print('In-arena private cursor -1/+1: Lua/native exit1 before evaluator/console/Run; unchanged archives PASS')

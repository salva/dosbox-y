#!/usr/bin/env python3
"""Used-target refusal then legitimate ordinary departure; never live reload.

Fresh restored-session terminal behavior is exercised by fresh-startup.py and
fresh-startup-terminal.py. This consumer verifies a refused live load does not
poison an ordinary session or prevent its program from returning to its shell.
"""
import os, pathlib
source=pathlib.Path(__file__).with_name('stopped-save.py').read_text()
source=source[:source.index('\nrun()')]
source=source.replace('debugbox TINY.COM\\n','debugbox TINY.COM\\necho RETURNED>RETURN.TXT\\nexit\\n')
start=source.index('  if slot_mode:\n   import runpy')
end=source.index('\n finally:',start)
source=source[:start]+'''  request('capture',f'STATEWRITE {w}/first.sav')
  refused=request('load',f'STATEREAD {w}/first.sav',expect_failure=True)
  assert b'fresh launch required' in refused,refused
  request('resave',f'STATEWRITE {w}/after-refusal.sav')
  with zipfile.ZipFile(w/'first.sav') as before,zipfile.ZipFile(w/'after-refusal.sav') as after:
   assert before.namelist()==after.namelist()
   for name in before.namelist():
    if name!='Time_Stamp':assert before.read(name)==after.read(name),name
  conn.sendall(b'REQ run EXEC RUN\\n')
  end=time.monotonic()+20
  while p.poll() is None:
   drain();assert time.monotonic()<end,('ordinary departure timeout',w);time.sleep(.02)
  drain()
  marker=w/'RETURN.TXT'
  assert p.returncode==0 and marker.read_text().strip()=='RETURNED',(p.returncode,w)
  print('ordinary session live-load refusal followed by legitimate return PASS',w,flush=True)
''' +source[end:]+ '\nrun()\n'
os.environ['DOSBOX_TEST_PROGRAM_HEX']='90 b8 00 4c cd 21 eb fe'
exec(compile(source,__file__,'exec'))

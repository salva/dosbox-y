#!/usr/bin/env python3
"""Finite real guest return through file or mapper restore, including no-Lua."""
import os, pathlib
source=pathlib.Path('tests/original-foundation/stopped-save.py').read_text()
source=source[:source.index('\nrun(False)')]
source=source.replace("bytes.fromhex('90 40 eb fc')","bytes.fromhex('b9 ff ff 40 e2 fd b8 00 4c cd 21')")
source=source.replace('debugbox TINY.COM\\n','debugbox TINY.COM\\necho RETURNED>RETURN.TXT\\nexit\\n')
start=source.index('  if slot_mode:\n   import runpy')
end=source.index('\n finally:',start)
source=source[:start]+'''  mode=os.environ['DEPARTURE']
  if slot_mode:
   import runpy
   keys=runpy.run_path('tests/original-foundation/slot-keys.py')
   sendkey=keys['key']
   request('before-slot',f'STATEWRITE {w}/first.sav')
   listener.close();conn.close();conn=None;time.sleep(2);drain()
   sendkey(':'+display,'s')
   deadline=time.monotonic()+15
   while not (d/'1.sav').exists() and time.monotonic()<deadline: time.sleep(.1);drain()
   assert (d/'1.sav').exists(),d
   time.sleep(1);drain()
   if mode!='control':
    sendkey(':'+display,'l');time.sleep(2);drain()
    assert b'Loading state from slot' in (d/'pty.log').read_bytes()
    assert b'Slot load refused' not in (d/'pty.log').read_bytes()
   os.write(master,b'RUN\\n')
  else:
   request('capture',f'STATEWRITE {w}/first.sav')
   if mode!='control':
    request('load',f'STATEREAD {w}/first.sav')
    request('resave',f'STATEWRITE {w}/restored.sav')
   conn.sendall(b'REQ run EXEC RUN\\n')
   if mode=='closed':time.sleep(.5);drain();conn.close();conn=None
  end=time.monotonic()+20
  while p.poll() is None:
   drain();assert time.monotonic()<end,('natural departure timeout',w);time.sleep(.02)
  drain()
  marker=w/'RETURN.TXT'
  if mode=='control':
   assert p.returncode==0 and marker.read_text().strip()=='RETURNED',(p.returncode,w)
  else:
   assert p.returncode==1 and not marker.exists(),(p.returncode,w)
  print('natural departure', 'slot' if slot_mode else 'file',mode,'PASS',p.returncode,w,flush=True)
''' +source[end:]+ '\nrun(False)\n'
exec(compile(source,__file__,'exec'))

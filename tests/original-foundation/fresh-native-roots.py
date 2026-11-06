#!/usr/bin/env python3
"""Run existing root-matrix captures through always-built native console.

No debugger/Agent/Lua in the consumer. Capture logs identify exact archives;
no recapture, mutation or inference from source-shape is used here.
"""
import hashlib, os, re, subprocess, tempfile, time
from pathlib import Path
source=Path(os.environ['DOSBOX_TEST_ROOT_MATRIX'])
work=Path(tempfile.mkdtemp(prefix='native-roots-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(work,flush=True)
conf=work/'matching.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
cases={'int22':(42,0,0),'int20':(0,0,0),'ah00':(0,0,0),'int27':(0,1,0),'ah31':(42,1,0),'self':(42,0,1),'child':(42,0,0),'child-tsr':(42,0,0),'shell-child':(42,0,0)}
for name,(code,resident,parent) in cases.items():
 archive=Path((source/(name+'-producer.log')).read_text().splitlines()[0])/'first.sav'
 before=hashlib.sha256(archive.read_bytes()).hexdigest()
 path=work/(name+'.log')
 with path.open('w') as log:
  with subprocess.Popen([os.environ['DOSBOX_TEST_NATIVE_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-restore-console'],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdin=subprocess.PIPE,stdout=log,stderr=log,text=True) as p:
   try:
    deadline=time.monotonic()+40
    while 'Restored console held:' not in path.read_text():
     assert p.poll() is None,path.read_text()
     assert time.monotonic()<deadline,'startup timeout'
     time.sleep(.05)
    time.sleep(.3)
    p.stdin.write('status\n');p.stdin.flush();time.sleep(.3)
    text=path.read_text();states=re.findall(r'Restored console held: (cycles=.*?ticks=\d+)',text)
    assert len(states)==2 and states[0]==states[1],text
    p.stdin.write('run\n');p.stdin.flush();assert p.wait(timeout=40)==0
   finally:
    if p.poll() is None:p.kill();p.wait()
 text=path.read_text()
 assert f'code={code} resident={resident} self-parent={parent}' in text,text
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
 print(name,'native held/status/explicit run/typed terminal disposal PASS',flush=True)

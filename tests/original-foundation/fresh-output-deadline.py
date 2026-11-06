#!/usr/bin/env python3
"""Integrated fresh FILE/INLINE timeout after retained native stream finalization."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
w=Path(tempfile.mkdtemp(prefix='output-deadline-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
archive=Path(os.environ['DOSBOX_TEST_ARCHIVE'])
before=hashlib.sha256(archive.read_bytes()).hexdigest()
subprocess.run(['cc','-shared','-fPIC',str(here/'output-deadline.c'),'-ldl','-o',str(w/'delay.so')],check=True)
conf=w/'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
for mode in ('file','code'):
 out=w/(mode+'.sav');out.write_bytes(b'previous destination')
 done=w/(mode+'-closed')
 script=f'''local d=require('dosbox')
local c=d.context()
local a=d.registers(c)
local r=d.save('{out}',100)
assert(r.effects.code=='timeout' and r.effects.retired and not r.effects.completed and not r.effects.published and r.effects.usable)
local b=d.registers(c)
assert(a.registers.cs==b.registers.cs and a.registers.ip==b.registers.ip)
local x <close> = setmetatable({{}},{{__close=function() local f=assert(io.open('{done}','w')); f:write('closed'); f:close() end}})
'''
 path=w/(mode+'.lua');path.write_text(script)
 r=subprocess.run([os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-'+mode,str(path) if mode=='file' else script],env=dict(os.environ,LD_PRELOAD=str(w/'delay.so'),DOSBOX_TEST_DELAY_OUTPUT=str(out),DOSBOX_TEST_DELAY_MARKER=str(w/(mode+'-fsync')),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
 (w/(mode+'.log')).write_bytes(r.stdout)
 assert r.returncode==0 and done.exists(),(w,mode,r.returncode,r.stdout.decode(errors='replace'))
 assert (w/(mode+'-fsync')).read_text()=='final fsync entered'
 assert out.read_bytes()==b'previous destination'
 assert not list(w.glob(mode+'.sav.tmp-*'))
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print(w,'FILE/INLINE final fsync deadline: exact retirement, no publication, old destination/source retained, close PASS')

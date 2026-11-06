#!/usr/bin/env python3
"""Actual initialized MSR/syscall equivalence and genuine instruction mismatches."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
work=Path(tempfile.mkdtemp(prefix='effective-instructions-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY']=os.environ['DOSBOX_TEST_BINARY']
print(work,flush=True)
def probe(label,script,extra):
 r=subprocess.run(['python3',str(here/script)],env=dict(env,**extra),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (work/(label+'.log')).write_text(r.stdout);r.check_returncode();return r.stdout
out=probe('producer','stopped-save.py',{'DOSBOX_TEST_INSTRUCTIONS':'enable msr=false\nenable syscall=true'})
archive=Path(out.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
probe('equivalent','fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_INSTRUCTIONS':'enable msr=false\nenable syscall=false\nmask stack pointer for enter leave instructions=false\nallow lmsw to exit protected mode=false\ninterruptible rep string op=4'})
marker=work/'evaluator-ran'
for label,switches in (('rep','enable msr=false\nenable syscall=false\ninterruptible rep string op=8'),('msr','enable msr=true\nenable syscall=false'),('syscall','enable msr=true\nenable syscall=true'),('cmpxchg','enable msr=false\nenable syscall=false\nenable cmpxchg8b=false'),('enter','enable msr=false\nenable syscall=false\nmask stack pointer for enter leave instructions=true'),('lmsw','enable msr=false\nenable syscall=false\nallow lmsw to exit protected mode=true')):
 conf=work/(label+'.conf')
 conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n'+switches+'\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 code='local f=assert(io.open('+repr(str(marker))+',"w")); f:write("ran"); f:close()'
 r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',code],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
 (work/(label+'.log')).write_text(r.stdout)
 assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
 assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
# Isolate syscall from MSR: this second producer keeps MSR enabled.
out=probe('msr-on-producer','stopped-save.py',{'DOSBOX_TEST_INSTRUCTIONS':'enable msr=true\nenable syscall=true'})
on_archive=Path(out.splitlines()[0])/'first.sav'
on_before=hashlib.sha256(on_archive.read_bytes()).hexdigest()
conf=work/'syscall-only.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\nenable msr=true\nenable syscall=false\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(on_archive),'-lua-driver-code',code],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
(work/'syscall-only.log').write_text(r.stdout)
assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
assert hashlib.sha256(on_archive.read_bytes()).hexdigest()==on_before
print('Initialized MSR-off syscall equivalence and ENTER/LMSW auto-to-false equivalence held/step/Run; MSR/CMPXCHG8B/ENTER/LMSW mismatch pre-evaluator disposal PASS')

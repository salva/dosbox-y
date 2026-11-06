#!/usr/bin/env python3
"""Owning effective CPU architecture and parsed serial, real fresh consumers."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
work=Path(tempfile.mkdtemp(prefix='effective-machine-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY']=os.environ['DOSBOX_TEST_BINARY']
print(work,flush=True)
def run(label,script,extra):
 r=subprocess.run(['python3',str(here/script)],env=dict(env,**extra),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150)
 (work/(label+'.log')).write_text(r.stdout);r.check_returncode();return r.stdout
def case(label,source,equivalent,mismatches):
 out=run(label+'-producer','stopped-save.py',{'DOSBOX_TEST_INSTRUCTIONS':source})
 archive=Path(out.splitlines()[0])/'first.sav'
 before=hashlib.sha256(archive.read_bytes()).hexdigest()
 run(label+'-equivalent','fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_INSTRUCTIONS':equivalent})
 for name,settings in mismatches:
  conf=work/(label+'-'+name+'.conf')
  conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n'+settings+'\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
  marker=work/'evaluator-ran'
  r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',f"io.open('{marker}','w'):close()"],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=40)
  (work/(label+'-'+name+'.log')).write_text(r.stdout)
  assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
  assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
# Mixed CPU ignores a PSN entirely; architecture still distinguishes 486.
case('inactive-serial','processor serial number=1111-2222-3333-4444',
     'processor serial number=aaaa-bbbb-cccc-dddd\nuse dynamic core with paging on=-1', [('architecture','cputype=486'),('cycles','cycles=fixed 4000'),('paging','use dynamic core with paging on=true')])
# Constructor parses separators, not literal spelling. Real distinct CPUID PSN refuses.
case('serial','cputype=pentium_iii\nprocessor serial number=1111-2222-3333-4444',
     'cputype=pentium_iii\nprocessor serial number=1111 2222 3333 4444',
     [('changed','cputype=pentium_iii\nprocessor serial number=1111-2222-3333-5555'),
      ('disabled','cputype=pentium_iii'),('prefetch','cputype=486_prefetch')])
print('initialized inactive/parsed serial equivalence held/step/Run; architecture/serial mismatches pre-evaluator disposal PASS')

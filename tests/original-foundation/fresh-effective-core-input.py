#!/usr/bin/env python3
"""Effective common-input identity and nearby core reset-policy aliases."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
work=Path(tempfile.mkdtemp(prefix='effective-core-input-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY']=os.environ['DOSBOX_TEST_BINARY']
print(work,flush=True)
def run(label,script,extra):
 r=subprocess.run(['python3',str(here/script)],env=dict(env,**extra),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=100)
 (work/(label+'.log')).write_text(r.stdout);r.check_returncode();return r.stdout
out=run('producer','stopped-save.py',{})
archive=Path(out.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
# This property has no native consumer at all: none of its spellings select
# hardware. Actual machine and aux/reset/device controls remain guarded.
for value in ('at','xt','pcjr','pc98'):
 run('controller-'+value,'fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_INSTRUCTIONS':'[keyboard]\ncontrollertype='+value+'\n[cpu]'})
for label,setting in [('address','memalias=36'),('cascade','cascade interrupt ignore in service=false'),('irq-delay','irq delay ns=838'),('combined','memalias=36\ncascade interrupt ignore in service=false\nirq delay ns=838\n[keyboard]\ncontrollertype=at')]:
 run(label,'fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_INSTRUCTIONS':'[dosbox]\n'+setting+'\n[cpu]'})
for label,keyboard,dosbox,success in [('matching','','',True),('explicit-at','controllertype=at','',True),('aux','aux=false','',False),('reset','allow output port reset=false','',False),('mouse','auxdevice=2button','',False),('address-other','','memalias=32',False),('cascade-other','','cascade interrupt ignore in service=true',False),('delay-other','','irq delay ns=1000',False)]:
 marker=work/(label+'-evaluator')
 conf=work/(label+'.conf')
 conf.write_text('[sdl]\noutput=surface\n[dosbox]\n'+dosbox+'\n[keyboard]\n'+keyboard+'\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 code='local f=assert(io.open('+repr(str(marker))+',"w")); f:write("ran"); f:close()'
 r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',code],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
 (work/(label+'.log')).write_text(r.stdout)
 if success: assert r.returncode==0 and marker.read_text()=='ran',r.stdout
 else:
  assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
  assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print('unconsumed controller spellings and resolved address/PIC policies held/step/Run; matching/equivalent evaluator controls; genuine aux/reset/type/address/PIC mismatch disposal; unchanged archive PASS')

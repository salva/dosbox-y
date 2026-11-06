#!/usr/bin/env python3
"""Initialized PSE equivalence through actual fresh held/step/Run consumers."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
work=Path(tempfile.mkdtemp(prefix='effective-pse-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY']=os.environ['DOSBOX_TEST_BINARY']
print(work,flush=True)
def probe(label,script,extra):
 r=subprocess.run(['python3',str(here/script)],env=dict(env,**extra),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (work/(label+'.log')).write_text(r.stdout);r.check_returncode();return r.stdout
out=probe('producer','stopped-save.py',{})
archive=Path(out.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
# Mixed architecture sorts above Pentium II in the original CPU owner, so
# auto resolves to PSE36 (not disabled). Test that actual owner decision.
for label,value in (('true','true'),('pse36','pse36')):
 probe(label,'fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_PSE':value})
marker=work/'evaluator-ran'
for value in ('none','false','pse','pse40'):
 conf=work/(value+'.conf')
 conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\nenable pse='+value+'\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 code='local f=assert(io.open('+repr(str(marker))+',"w")); f:write("ran"); f:close()'
 r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',code],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
 (work/(value+'.log')).write_text(r.stdout)
 assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
 assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print('PSE auto/true/pse36 equivalence: held/step/Run/root disposal; none/false/PSE/PSE40 mismatch before evaluator; unchanged archive PASS')

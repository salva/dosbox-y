#!/usr/bin/env python3
"""Real CPUID leaf-zero consumer, effective default/truncation and refusal."""
import hashlib, os
from pathlib import Path
import subprocess, tempfile
here = Path(__file__).resolve().parent
work = Path(tempfile.mkdtemp(prefix='effective-cpuid-', dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env = {k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY'] = os.environ['DOSBOX_TEST_BINARY']
subprocess.run(['nasm','-f','bin',str(here/'fresh-cpuid.asm'),'-o',str(work/'guest.com')],check=True)
print(work,flush=True)
def probe(name, script, extra):
 r=subprocess.run(['python3',str(here/script)],env=dict(env,**extra),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (work/(name+'.log')).write_text(r.stdout)
 r.check_returncode()
 return r.stdout
out=probe('producer','stopped-save.py',{'DOSBOX_TEST_PROGRAM_HEX':(work/'guest.com').read_bytes().hex(),'DOSBOX_TEST_REFUSAL_REGISTERS':'CS:IP=0814:0101'})
archive=Path(out.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
for label,value in (('explicit','GenuineIntel'),('truncated','GenuineIntelIgnoredSuffix')):
 probe(label,'fresh-startup.py',{'DOSBOX_TEST_ARCHIVE':str(archive),'DOSBOX_TEST_CPUID':value,'DOSBOX_TEST_ROOT_EXIT':'1'})
conf=work/'mismatch.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\ncpuid string=AuthenticAMD\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
marker=work/'evaluator-ran'
script="local f=assert(io.open("+repr(str(marker))+",'w')); f:write('ran'); f:close()"
r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-inline',script],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
(work/'mismatch.log').write_text(r.stdout)
assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print('default/explicit/truncated vendor: held/step/CPUID/root42 PASS; differing initialized vendor refused/disposed; no evaluator; source unchanged',flush=True)

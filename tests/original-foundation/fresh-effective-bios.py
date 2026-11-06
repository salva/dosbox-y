#!/usr/bin/env python3
"""Real fresh BIOS effective equivalence and genuine native-policy refusal."""
import hashlib, os, subprocess, tempfile
from pathlib import Path
here=Path(__file__).resolve().parent
work=Path(tempfile.mkdtemp(prefix='effective-bios-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_BINARY']=os.environ['DOSBOX_TEST_BINARY']
print(work,flush=True)
def owner_setting(setting):
 if setting.startswith(('isa memory hole','unhandled irq')):
  return '[dosbox]\n'+setting+'\n[cpu]'
 return setting
def probe(label,extra):
 extra=owner_setting(extra)
 r=subprocess.run(['python3',str(here/'fresh-startup.py')],env=dict(env,DOSBOX_TEST_ARCHIVE=str(archive),DOSBOX_TEST_INSTRUCTIONS=extra),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
 (work/(label+'.log')).write_text(r.stdout);r.check_returncode()
r=subprocess.run(['python3',str(here/'stopped-save.py')],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90)
(work/'producer.log').write_text(r.stdout);r.check_returncode()
archive=Path(r.stdout.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
for label,setting in [('pnp','isapnpport=true'),('apm','apmbios version=1.2'),('holes','isa memory hole at 512kb=false\nisa memory hole at 15mb=false'),('irq','unhandled irq handler=simple')]:
 probe(label,setting)
# Title alone is a host frontend difference. Matching control also proves the
# marker is reachable, rather than treating early process exit as success.
for label,setting,title,success in [('matching','','',True),('title','','independent-assessor-window-title',True),('pnp-off','isapnpport=false','',False),('apm-old','apmbios version=1.1','',False),('apm-mode','apmbios allow 16-bit protected mode=false','',False),('hole','isa memory hole at 512kb=true','',False),('irq-other','unhandled irq handler=cooperative_2nd','',False),('integration','integration device=true','',False)]:
 marker=work/(label+'-evaluator')
 conf=work/(label+'.conf')
 conf.write_text('[sdl]\noutput=surface\n[dosbox]\ntitle='+title+'\n[cpu]\ncore=normal\ncycles=fixed 3000\n'+owner_setting(setting)+'\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 code='local f=assert(io.open('+repr(str(marker))+',"w")); f:write("ran"); f:close()'
 r=subprocess.run([env['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',code],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=45)
 (work/(label+'.log')).write_text(r.stdout)
 if success: assert r.returncode==0 and marker.read_text()=='ran',r.stdout
 else:
  assert r.returncode==1 and 'Config_Identity: incompatible or malformed' in r.stdout,r.stdout
  assert 'disposable target cannot continue' in r.stdout and not marker.exists(),r.stdout
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print('BIOS auto/resolved PnP/APM/holes/IRQ held/step/Run; title/control evaluator; native-policy mismatch refusal and unchanged archive PASS')

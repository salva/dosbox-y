#!/usr/bin/env python3
"""Debugger capture -> no-debug/no-Agent/no-Lua console held -> explicit run."""
import hashlib, os, pathlib, subprocess, tempfile, time, re
here=pathlib.Path(__file__).resolve().parent
work=pathlib.Path(tempfile.mkdtemp(prefix='native-console-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(work,flush=True)
env={k:v for k,v in os.environ.items() if not k.startswith('DOSBOX_TEST_')}
fixture=os.environ.get('DOSBOX_TEST_CONSOLE_FIXTURE','sb-fifo')
assert fixture in ('sb-fifo','font','ah86','pause'), fixture
setup={
 'sb-fifo':{'DOSBOX_TEST_SB_HELD':'1'},
 'font':{'DOSBOX_TEST_FONT':'1'},
 'ah86':{'DOSBOX_TEST_AH86':'1','DOSBOX_TEST_AH86_ACTIVE':'1'},
 'pause':{'DOSBOX_TEST_PAUSE':'1'},
}[fixture]
producer=dict(env,**setup,DOSBOX_TEST_BINARY=os.environ['DOSBOX_TEST_BINARY'],DOSBOX_TEST_AUDIO='1',DOSBOX_TEST_CAPTURE_IP='0814:0200',DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0201')
r=subprocess.run(['python3',str(here/'stopped-save.py')],env=producer,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
(work/'producer.log').write_text(r.stdout)
r.check_returncode()
archive=pathlib.Path(r.stdout.splitlines()[0])/'first.sav'
before=hashlib.sha256(archive.read_bytes()).hexdigest()
conf=work/'fresh.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
args=[os.environ['DOSBOX_TEST_NATIVE_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-restore-console']
with (work/'native.log').open('w') as log:
 with subprocess.Popen(args,env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdin=subprocess.PIPE,stdout=log,stderr=log,text=True) as p:
  try:
   deadline=time.monotonic()+40
   while 'Restored console held:' not in (work/'native.log').read_text():
    assert p.poll() is None,(work/'native.log').read_text()
    assert time.monotonic()<deadline,'startup timeout'
    time.sleep(.05)
   time.sleep(2)
   p.stdin.write('status\n');p.stdin.flush()
   time.sleep(.3)
   text=(work/'native.log').read_text()
   states=re.findall(r'Restored console held: (cycles=.*?ticks=\d+)',text)
   assert len(states)==2 and states[0]==states[1],text
   assert 'Restored session ended' not in text
   p.stdin.write('run\n');p.stdin.flush()
   assert p.wait(timeout=40)==0
  finally:
   if p.poll() is None:p.kill();p.wait()
text=(work/'native.log').read_text()
assert 'code=42 resident=0 self-parent=0' in text,text
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
# Original slot safety remains authoritative: mutate first Shell role slot with
# valid ZIP CRC, so native mismatch must refuse before console attachment.
import zipfile
bad=work/'wrong-slot.sav'
with zipfile.ZipFile(archive) as source, zipfile.ZipFile(bad,'w') as dest:
 for item in source.infolist():
  data=source.read(item.filename)
  if item.filename=='Shell bindings':
   data=bytes([data[0]^1])+data[1:]
  dest.writestr(item,data)
negative=subprocess.run([args[0],'-conf',str(conf),'-restore-state',str(bad),'-restore-console'],env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),input='run\n',text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
(work/'wrong-slot.log').write_text(negative.stdout)
assert negative.returncode==1 and 'State component Shell bindings failed during installation' in negative.stdout,negative.stdout
assert 'Restored console held:' not in negative.stdout and 'Restored session ended' not in negative.stdout
quit_result=subprocess.run(args,env=dict(env,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),input='quit\n',text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=40)
(work/'quit.log').write_text(quit_result.stdout)
assert quit_result.returncode==1 and 'Restored console quit; target discarded' in quit_result.stdout
assert 'Restored session ended' not in quit_result.stdout
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print(f'native held/delay/status invariant; explicit Run/{fixture}/root42/disposal; mismatch/quit disposal; source unchanged PASS')

"""Actual fresh-startup stopped import/observe/delay/save/explicit-step probe.
Uses an archive from the stopped-save.py producer with a compatible engine/ABI
and matching preconfigured guest hardware (binary/frontend equality not required).
This is a core witness, not backend/audio or universal native-binding acceptance.
"""
import os,pathlib,tempfile,socket,subprocess,time,select,zipfile,hashlib
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root/'fresh-native-entry'))
archive=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
before=hashlib.sha256(archive.read_bytes()).hexdigest()
changed_entry=None
if os.environ.get('DOSBOX_TEST_SEARCH_CHANGED_ENTRY'):
 changed_entry=archive.parent/'NEW-HOST.TXT'
 assert not changed_entry.exists()
 changed_entry.write_bytes(b'current directory contents, not archive rollback\n')
if os.environ.get('DOSBOX_TEST_SEARCH_MATCHING_ENTRY'):
 changed_entry=archive.parent/'ZZZ.DAT'
 if changed_entry.exists():
  assert changed_entry.read_bytes()==b'current directory contents, not archive rollback\n'
 else:
  changed_entry.write_bytes(b'current directory contents, not archive rollback\n')
startup_archive=archive
if os.environ.get('DOSBOX_TEST_IVT_HOOKS') or os.environ.get('DOSBOX_TEST_XMS_HOOK'):
 # Guest hooks are payload, not native binding identity. Preserve original
 # input; create one valid-CRC fixture with hooks outside controlled spans.
 import re,struct
 _,descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(archive.parent/'layout.txt').read_text()))
 startup_archive=w/'hooked.sav'
 xms_hook=None
 with zipfile.ZipFile(archive) as src,zipfile.ZipFile(startup_archive,'w',zipfile.ZIP_STORED) as dst:
  if os.environ.get('DOSBOX_TEST_XMS_HOOK'):
   fields=struct.unpack('<6I',src.read('XMS')[:24])
   assert fields[0] and fields[2]
   xms_hook=(fields[2]>>16)*16+(fields[2]&0xffff)
  for name in src.namelist():
   data=src.read(name)
   if name=='Memory':
    data=bytearray(data)
    if os.environ.get('DOSBOX_TEST_IVT_HOOKS'):
     struct.pack_into('<II',data,descriptor+0x22*4,0x08140100,0x08140101)
     struct.pack_into('<I',data,descriptor+0x2e*4,0x08140100)
     # These guest vectors must not be mistaken for constructor-owned native
     # trampoline bytes. The speaker continuation does not invoke either hook.
     if os.environ.get('DOSBOX_TEST_DEVICE_IVT_HOOKS'):
      for vector in (0x10,0x33):
       struct.pack_into('<I',data,descriptor+vector*4,0x08140100)
    if xms_hook is not None:
     # Five guest-owned bytes, not the FE38 native dispatch after them.
     data[descriptor+xms_hook:descriptor+xms_hook+5]=b'\x90'*5
   dst.writestr(name,data)
startup_hash=hashlib.sha256(startup_archive.read_bytes()).hexdigest()
capture_ip=os.environ.get('DOSBOX_TEST_STEP_IP','0814:0101').encode()
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(25)
conf=w/'fixture.conf';conf.write_text(f'[dosbox]\nmcp_server={listener.getsockname()[1]}\n[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
if os.environ.get('DOSBOX_TEST_XMS_HMA_HELD'):
 conf.write_text(conf.read_text()+'[dos]\nems='+('true' if os.environ.get('DOSBOX_TEST_HMA_WITH_EMS') else 'false')+'\n[config]\ndos=low,umb\n')
if os.environ.get('DOSBOX_TEST_MEMSIZE'):
 memory=int(os.environ['DOSBOX_TEST_MEMSIZE'])
 assert 1 <= memory <= 256
 conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nmemsize={memory}\n'))
if os.environ.get('DOSBOX_TEST_MEMSIZEKB'):
 memory=int(os.environ['DOSBOX_TEST_MEMSIZEKB'])
 assert 1 <= memory <= 262144
 conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nmemsize=0\nmemsizekb={memory}\n'))
if os.environ.get('DOSBOX_TEST_CYCLES'):
 cycles=int(os.environ['DOSBOX_TEST_CYCLES'])
 assert 1 <= cycles <= 1000000
 conf.write_text(conf.read_text().replace('cycles=fixed 3000',f'cycles=fixed {cycles}'))
if os.environ.get('DOSBOX_TEST_CLEAR_TF'):
 conf.write_text(conf.read_text().replace('[cpu]\n','[cpu]\nclear trap flag on unhandled int 1=true\n'))
if os.environ.get('DOSBOX_TEST_AUTO_CYCLES'):
 conf.write_text(conf.read_text().replace('cycles=fixed 3000','cycles=auto'))
if os.environ.get('DOSBOX_TEST_PRESENTATION'):
 # Deliberately different destination host presentation; guest raster/font
 # switches remain unchanged and emulated audio is still enabled separately.
 conf.write_text(conf.read_text()+'[render]\nscaler=none\naspect=true\naspect_ratio=16:9\nframeskip=1\n')
if os.environ.get('DOSBOX_TEST_BASIC_ROM'):
 rom=pathlib.Path(os.environ.get('DOSBOX_TEST_BASIC_ROM_PATH',str(archive.parent/'BASIC.ROM')))
 rom.write_bytes(bytes([0x90])*32768)
 conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nibm rom basic={rom}\n'))
if os.environ.get('DOSBOX_TEST_CAPTURE_POLICY'):
 conf.write_text(conf.read_text()+'[dosbox]\nsaveslot=97\nsavefile=unused-policy.sav\nsaveremark=false\nautosave=-30 1-3\nforceloadstate=true\n')
if os.environ.get('DOSBOX_TEST_AUDIO'):
 conf.write_text(conf.read_text().replace('nosound=true','nosound=false'))
log=(w/'native.log').open('wb')
p=subprocess.Popen([os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(startup_archive)],stdout=log,stderr=log,env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',TERM='xterm'))
conn=None;buf=b''
def request(ident,command,accepted=True):
 global buf
 conn.sendall(f'REQ {ident} EXEC {command}\n'.encode())
 marker=f'END {ident}\n'.encode();deadline=time.monotonic()+30
 while marker not in buf:
  if time.monotonic()>deadline:raise RuntimeError(('response timeout',buf))
  if select.select([conn],[],[],.05)[0]:
   data=conn.recv(65536)
   if not data:raise RuntimeError(('EOF',buf))
   buf+=data
 out,buf=buf.split(marker,1)
 with (w/'wire.log').open('ab') as f:f.write(out+marker)
 assert f'BEGIN {ident} {"OK" if accepted else "ERR"}\n'.encode() in out,out
 return out
try:
 conn,_=listener.accept()
 time.sleep(3)
 if os.environ.get('DOSBOX_TEST_LIVE_REFUSAL'):
  refused=request('live-load',f'STATEREAD {archive}',False)
  assert b'fresh' in refused.lower(),refused
 request('save1',f'STATEWRITE {w}/one.sav')
 time.sleep(2)
 request('save2',f'STATEWRITE {w}/two.sav')
 with zipfile.ZipFile(startup_archive) as source,zipfile.ZipFile(w/'one.sav') as a,zipfile.ZipFile(w/'two.sav') as b:
  # The exact binary's extracted extents delimit guest CPU state and RAM;
  # native paging caches and memory handler pointers are reconstructed.
  import re
  extents=list(map(int,re.findall(r'\$\d+ = (\d+)',(archive.parent/'layout.txt').read_text())))
  cpu_prefix,memory_descriptor=extents
  assert source.read('CPU')[:cpu_prefix]==a.read('CPU')[:cpu_prefix]
  assert source.read('Memory')[memory_descriptor:]==a.read('Memory')[memory_descriptor:]
  for name in ('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10','Mixer'):
   assert a.read(name)==b.read(name),('held state changed across delay',name)
  for name in ('Pic','Dosbox-x','Keyboard','IntTimer10'):
   assert source.read(name)==a.read(name),('guest owner changed during startup',name)
  # Mixer contains legacy native pointers (including OPL operator wiring).
  # Cross-process byte equality is not a semantic audio assertion. The held
  # one/two comparison above still requires no mutation during host delay;
  # meaningful device continuation needs its own workload witness.
 if os.environ.get('DOSBOX_TEST_FILE_STATE'):
  request('witness-bp','BP 0814:0208')
  request('witness-run','RUN')
  time.sleep(2)
  request('witness-save',f'STATEWRITE {w}/witness.sav')
  with zipfile.ZipFile(w/'witness.sav') as state:
   import struct
   ram=state.read('Memory');mark=ram.index(b'R3-FILE-WITNESS!')+len(b'R3-FILE-WITNESS!')
   values=struct.unpack_from('<5H',ram,mark)
   assert values==(0x1234,4,7,5,18),values
   assert {ram[mark+14:mark+27].split(b'\0',1)[0],
           ram[mark+27:mark+40].split(b'\0',1)[0]}=={b'ONE.DAT',b'TWO.DAT'}
   assert ram[mark+10:mark+14]==b'nged',ram[mark+10:mark+14]
  assert (archive.parent/'FLAG.TXT').read_bytes()==b'changed between processes\r\n'
  print(w,'fresh file read-only handle/cursor/new current-directory search passed')
 elif os.environ.get('DOSBOX_TEST_NATIVE_DEPARTURE'):
  step=request('step','STEPIN');assert b'CS:IP='+capture_ip in step,step
  conn.sendall(b'REQ departure EXEC RUN\n')
  assert p.wait(timeout=15)==1
  text=(w/'native.log').read_text(errors='replace')
  assert 'Restored session unsupported native departure (signal 3); target discarded' in text,text[-3000:]
  assert 'Restored session ended:' not in text
  print(w,'restored native reset refused without ordinary reboot fallback')
 elif os.environ.get('DOSBOX_TEST_ROOT_EXIT'):
  step=request('step','STEPIN');assert b'CS:IP='+capture_ip in step,step
  # Default program is NOP; MOV AX,4C2A; INT21. Optional prefix steps
  # admit alternate fixtures without changing the held-image assertions.
  for index in range(int(os.environ.get('DOSBOX_TEST_EXIT_PREFIX_STEPS','1'))):
   request('prefix'+str(index),'STEPIN')
  command=os.environ.get('DOSBOX_TEST_EXIT_COMMAND','RUN')
  assert command in ('RUN','STEPIN')
  if command=='STEPIN':
   # INT instruction and native interrupt trampoline are separate decoder
   # stops. Step each until the root cut closes the disposable invocation.
   for index in range(64):
    try:request('exit'+str(index),'STEPIN')
    except RuntimeError as error:
     if error.args and isinstance(error.args[0],tuple) and error.args[0][0]=='EOF':break
     raise
   else:raise AssertionError('root did not terminate within 64 explicit steps')
  else:conn.sendall(b'REQ exit EXEC RUN\n')
  assert p.wait(timeout=15)==0
  text=(w/'native.log').read_text(errors='replace')
  expected=os.environ.get('DOSBOX_TEST_ROOT_FACTS','code=42 resident=0 self-parent=0')
  assert 'root=0814 '+expected in text,text[-3000:]
  assert 'DOS kernel corruption' not in text
  assert 'UNSAFE_ROOT_INT22' not in text
  print(w,'actual restored DOS_Terminate root typed unwind passed')
 else:
  step=request('step','STEPIN');assert b'CS:IP='+capture_ip in step,step
  print(step.decode())
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
 assert hashlib.sha256(startup_archive.read_bytes()).hexdigest()==startup_hash
 if changed_entry:
  assert changed_entry.read_bytes()==b'current directory contents, not archive rollback\n'
 print(w,'actual fresh import stayed stopped across two saves/host delay; explicit continuation passed; source unchanged')
finally:
 if conn:conn.close()
 listener.close();p.terminate()
 try:p.wait(timeout=10)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 log.close()

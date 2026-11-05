"""Stopped-save producer and used-target load refusal integration probe.
Fresh success/malformed-owner disposal belongs to fresh-startup{,-failure}.py.
"""
import os, pathlib, tempfile, subprocess, pty, fcntl, termios, struct, socket, select, time, zipfile, re, json, hashlib
root = pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'stopped-save-vertical'
root.mkdir(exist_ok=True)
w = pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root))
binary = pathlib.Path(os.environ.get('DOSBOX_TEST_BINARY','src/dosbox-x')).resolve()
(w/'TINY.COM').write_bytes(bytes.fromhex(os.environ.get('DOSBOX_TEST_PROGRAM_HEX','90 40 eb fc')))
if os.environ.get('DOSBOX_TEST_IHSEG'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-ihseg.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_EXEC_CHILD'):
 subprocess.run(['nasm','-f','bin',*(['-DCHILD_TSR'] if os.environ.get('DOSBOX_TEST_CHILD_TSR') else []),*(['-DCHILD_SHELL'] if os.environ.get('DOSBOX_TEST_CHILD_SHELL') else []),str(pathlib.Path(__file__).with_name('fresh-child.asm')),'-o',str(w/'TINY.COM')],check=True)
 (w/'CHILD.COM').write_bytes(bytes.fromhex(os.environ.get('DOSBOX_TEST_CHILD_HEX','b8 07 4c cd 21')))
if os.environ.get('DOSBOX_TEST_XMS_SERVICE'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-xms.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_UMB_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-umb-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_XMS_HMA_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-xms-hma-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_XMS_A20_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-xms-a20-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_HMA_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-hma-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_XMS_HELD'):
 subprocess.run(['nasm','-f','bin',*(['-DGUEST_HOOK'] if os.environ.get('DOSBOX_TEST_XMS_GUEST_HOOK') else []),str(pathlib.Path(__file__).with_name('fresh-xms-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_SB_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-sb-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_EMS_SERVICE'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-ems.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_EMS_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-ems-held.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_CUSTOM_INT22'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-int22.asm')),'-o',str(w/'TINY.COM')],check=True)
if os.environ.get('DOSBOX_TEST_INT27'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-int27.asm')),'-o',str(w/'TINY.COM')],check=True)
for option,fixture in (('SPEAKER','fresh-speaker.asm'),('VIDEO','fresh-video.asm'),('KEYBOARD','fresh-keyboard.asm'),('MOUSE','fresh-mouse.asm')):
 if os.environ.get('DOSBOX_TEST_'+option):
  subprocess.run(['nasm','-f','bin',*(['-DCAPTURE_ACTIVE'] if os.environ.get('DOSBOX_TEST_'+option+'_ACTIVE') else []),*(['-DVESA_SERVICE'] if option=='VIDEO' and os.environ.get('DOSBOX_TEST_VESA_SERVICE') else []),str(pathlib.Path(__file__).with_name(fixture)),'-o',str(w/'TINY.COM')],check=True)
slot_mode=bool(os.environ.get('DOSBOX_TEST_SLOT'))
file_state = bool(os.environ.get('DOSBOX_TEST_FILE_STATE'))
unsupported_devices = bool(os.environ.get('DOSBOX_TEST_UNSUPPORTED_DEVICES'))
if file_state:
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('file-state.asm')),'-o',str(w/'TINY.COM')],check=True)
 for name in ('ONE.DAT', 'TWO.DAT'): (w/name).write_bytes(b'directory witness')
if os.environ.get('DOSBOX_TEST_SEARCH_HELD'):
 subprocess.run(['nasm','-f','bin','-DEXTRA_CURRENT_ENTRY='+str(int(bool(os.environ.get('DOSBOX_TEST_SEARCH_MATCHING_ENTRY')))),'-DDELETE_CURRENT_ENTRY='+str(int(bool(os.environ.get('DOSBOX_TEST_SEARCH_DELETED_ENTRIES')))),str(pathlib.Path(__file__).with_name('fresh-search-held.asm')),'-o',str(w/'TINY.COM')],check=True)
 (w/'ONE.DAT').write_bytes(b'one')
 (w/'TWO.DAT').write_bytes(b'two')
if os.environ.get('DOSBOX_TEST_LFN_HELD'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-lfn-held.asm')),'-o',str(w/'TINY.COM')],check=True)
 (w/'ONE.DAT').write_bytes(b'one')
if os.environ.get('DOSBOX_TEST_SHARED_FILE'):
 subprocess.run(['nasm','-f','bin',str(pathlib.Path(__file__).with_name('fresh-file-shared.asm')),'-o',str(w/'TINY.COM')],check=True)
flagged = file_state or bool(os.environ.get('DOSBOX_TEST_FLAGGED')) or bool(os.environ.get('DOSBOX_TEST_SHARED_FILE'))
if flagged:
 (w/'FLAG.TXT').write_bytes(b'captured flagged guest file\r\n')
# Obtain native wire extents from this exact binary, not guessed pointer offsets.
# The CPU prefix ends after the lazy flags, before process-local paging caches.
size_expr = ('sizeof(cpu_regs)+sizeof(cpu.cpl)+sizeof(cpu.mpl)+sizeof(cpu.cr0)+sizeof(cpu.pmode)'
 '+sizeof(cpu.gdt.table_base)+sizeof(cpu.gdt.table_limit)+sizeof(cpu.gdt.ldt_base)'
 '+sizeof(cpu.gdt.ldt_limit)+sizeof(cpu.gdt.ldt_value)+sizeof(cpu.idt.table_base)'
 '+sizeof(cpu.idt.table_limit)+sizeof(cpu.stack)+sizeof(cpu.code)+sizeof(cpu.hlt.cs)'
 '+sizeof(cpu.hlt.eip)+sizeof(cpu.exception)+sizeof(cpu.direction)+sizeof(cpu.trap_skip)'
 '+sizeof(cpu.drx)+sizeof(cpu.trx)+sizeof(Segs)+sizeof(CPU_Cycles)+sizeof(CPU_CycleLeft)'
 '+sizeof(CPU_IODelayRemoved)+sizeof(cpu_tss.desc.saved)+sizeof(cpu_tss.selector)'
 '+sizeof(cpu_tss.base)+sizeof(cpu_tss.limit)+sizeof(cpu_tss.is386)+sizeof(cpu_tss.valid)'
 '+sizeof(lastint)+sizeof(uint16_t)+sizeof(lflags)+sizeof(paging.cr3)+sizeof(paging.cr2)+sizeof(paging.base)')
layout = subprocess.check_output(['gdb','-batch','-ex','p '+size_expr,'-ex','p sizeof(memory)',str(binary)],text=True)
(w/'layout.txt').write_text(layout)
cpu_prefix, memory_descriptor = map(int,re.findall(r'\$\d+ = (\d+)',layout))
print(w, flush=True)
def run():
 d=w/'capture';d.mkdir()
 listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(20)
 flag_command = 'flagsave FLAG.TXT\n' if flagged else ''
 conf=d/'fixture.conf';conf.write_text(f'[dosbox]\nmcp_server={listener.getsockname()[1]}\n[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c {w}\nc:\n{flag_command}debugbox TINY.COM\n')
 # Exercise represented geometry/timing, not just the default 16MB/3000 case.
 if os.environ.get('DOSBOX_TEST_MEMSIZE'):
  memory=int(os.environ['DOSBOX_TEST_MEMSIZE'])
  assert 1 <= memory <= 256
  conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nmemsize={memory}\n'))
 if os.environ.get('DOSBOX_TEST_CLEAR_TF'):
  conf.write_text(conf.read_text().replace('[cpu]\n','[cpu]\nclear trap flag on unhandled int 1=true\n'))
 if os.environ.get('DOSBOX_TEST_AUTO_CYCLES'):
  conf.write_text(conf.read_text().replace('cycles=fixed 3000','cycles=auto'))
 if os.environ.get('DOSBOX_TEST_CYCLES'):
  cycles=int(os.environ['DOSBOX_TEST_CYCLES'])
  assert 1 <= cycles <= 1000000
  conf.write_text(conf.read_text().replace('cycles=fixed 3000',f'cycles=fixed {cycles}'))
 if os.environ.get('DOSBOX_TEST_FPU'):
  conf.write_text(conf.read_text().replace('[cpu]\n','[cpu]\nfpu='+os.environ['DOSBOX_TEST_FPU']+'\n'))
 if os.environ.get('DOSBOX_TEST_AUDIO'):
  conf.write_text(conf.read_text().replace('nosound=true','nosound=false'))
 if os.environ.get('DOSBOX_TEST_BASIC_ROM'):
  rom=w/'BASIC.ROM';rom.write_bytes(bytes([0x90])*32768)
  conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nibm rom basic={rom}\n'))
 # Exercise HMA ownership independently and with the original EMS/VCPI owner.
 if os.environ.get('DOSBOX_TEST_XMS_HMA_HELD'):
  conf.write_text(conf.read_text().replace('[autoexec]', '[dos]\nems='+('true' if os.environ.get('DOSBOX_TEST_HMA_WITH_EMS') else 'false')+'\n[config]\ndos=low,umb\n[autoexec]'))
 if os.environ.get('DOSBOX_TEST_LFN_HELD'):
  conf.write_text(conf.read_text().replace('[autoexec]', '[dos]\nver=7.1\nlfn=true\n[autoexec]'))
 if unsupported_devices:
  conf.write_text(conf.read_text().replace('[autoexec]', '[devices]\nrem unrepresented driver recipe\n[autoexec]'))
 if slot_mode: conf.write_text(conf.read_text().replace('[dosbox]\n',f'[dosbox]\nsaveremark=false\n'))
 xr,xw=os.pipe();xlog=open(d/'xvfb.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(xw),'-screen','0','1024x768x24'],pass_fds=(xw,),stdout=xlog,stderr=xlog);os.close(xw)
 with os.fdopen(xr) as pipe: display=pipe.readline().strip()
 master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(master,False)
 log=open(d/'pty.log','wb');wire=open(d/'wire.log','wb')
 argv=[str(binary),'-conf',str(conf)]+(['-savedir',str(d)] if slot_mode else [])
 if os.environ.get('DOSBOX_SAVE_BACKTRACE'):
  argv=['gdb','-batch','-ex','break '+os.environ['DOSBOX_SAVE_BACKTRACE'],'-ex','run','-ex','bt']
  for expression in os.environ.get('DOSBOX_SAVE_PROBE','').split(';'):
   if expression:argv+=['-ex',expression]
  argv+=['-ex','continue','--args']+[str(binary),'-conf',str(conf)]+(['-savedir',str(d)] if slot_mode else [])
 p=subprocess.Popen(argv,stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER=os.environ.get('DOSBOX_TEST_AUDIO_DRIVER','dummy'),SDL_DISKAUDIOFILE=str(d/'host-audio.raw')));os.close(slave)
 conn=None;buf=b''
 def drain():
  while select.select([master],[],[],0)[0]:
   try: data=os.read(master,65536)
   except OSError: break
   if not data:break
   log.write(data);log.flush()
 def request(ident,command,expect_failure=False):
  nonlocal buf
  text=(f'REQ {ident} BREAK\n' if command=='BREAK' else f'REQ {ident} EXEC {command}\n').encode();conn.sendall(text);wire.write(text);wire.flush()
  marker=f'END {ident}\n'.encode();end=time.monotonic()+120
  while marker not in buf and time.monotonic()<end:
   drain()
   if select.select([conn],[],[],.02)[0]:
    data=conn.recv(65536)
    if not data:raise RuntimeError('bridge EOF')
    buf+=data
  assert marker in buf,buf
  out,buf=buf.split(marker,1);wire.write(out+marker);wire.flush();print(out.decode(),flush=True)
  expected = f'BEGIN {ident} {"ERR" if expect_failure else "OK"}\n'.encode()
  while out.startswith(b'BEGIN event '):
   event,out=out.split(b'END event\n',1)
   assert b'execution_terminal' in event and b'failed=0' in event,event
  assert out.startswith(expected), out
  if command.startswith(('STATEWRITE ', 'STATEREAD ')):
   assert b'Debugger command not recognized' not in out, out
   assert (b'State failed' if expect_failure else b'State completed') in out, out
   assert (b'State completed' if expect_failure else b'State failed') not in out, out
  return out
 try:
  conn,_=listener.accept();conn.setblocking(False)
  time.sleep(2);drain()
  if os.environ.get('DOSBOX_TEST_BASIC_ROM'):
   # Change only the pathname contents after initialization. The archive must
   # continue describing the bytes actually loaded by the BIOS owner.
   request('rom-before',f'STATEWRITE {d}/rom-before.sav')
   (w/'BASIC.ROM').write_bytes(bytes([0x91])*32768)
   request('rom-after',f'STATEWRITE {d}/rom-after.sav')
   with zipfile.ZipFile(d/'rom-before.sav') as before, zipfile.ZipFile(d/'rom-after.sav') as after:
    assert before.read('Config_Identity')==after.read('Config_Identity')
    assert before.read('Build_Identity')==after.read('Build_Identity')
  if os.environ.get('DOSBOX_TEST_AUDIO_DRIVER'):
   driver=os.environ['DOSBOX_TEST_AUDIO_DRIVER'].encode()
   assert b'Audio '+driver in (d/'pty.log').read_bytes(), 'requested SDL audio backend not initialized'
   if driver==b'disk':
    assert (d/'host-audio.raw').stat().st_size>0, 'SDL disk backend produced no host output'
  if unsupported_devices:
   # Real initialized process, not the reduced Section_line fixture. Refusal
   # must not publish a destination or leave a serializer-owned partial file.
   destination=w/'unsupported.sav'
   request('devices-save',f'STATEWRITE {destination}',expect_failure=True)
   assert b'nonempty [devices]' in (d/'pty.log').read_bytes()
   assert not destination.exists()
   assert not list(w.glob(destination.name+'.*')),list(w.iterdir())
   step=request('devices-step','STEPIN')
   assert b'CS:IP=0814:0101' in step,step
   return
  if slot_mode:
   import runpy, shutil
   sendkey=runpy.run_path(str(pathlib.Path(__file__).with_name('slot-keys.py')))['key']
   # Bridge departure is required before the Interactive slot owns admission.
   request('pre-slot',f'STATEWRITE {d}/before-slot.sav')
   address=listener.getsockname();listener.close();conn.close();conn=None
   time.sleep(2);drain()
   sendkey(':'+display,'s')
   deadline=time.monotonic()+15
   while not (d/'1.sav').exists() and time.monotonic()<deadline: time.sleep(.1);drain()
   assert (d/'1.sav').exists(),(d/'pty.log').read_bytes()[-3000:]
   time.sleep(1);drain()
   shutil.copyfile(d/'1.sav',d/'slot-before.sav')
   sendkey(':'+display,'l');time.sleep(2);drain()
   text=(d/'pty.log').read_bytes()
   assert b'fresh launch required' in text and b'Loading state from slot' not in text,text[-4000:]
   # Refused load retains the held guest and repeated slot save capability.
   sendkey(':'+display,'s');time.sleep(2);drain()
   with zipfile.ZipFile(d/'slot-before.sav') as before, zipfile.ZipFile(d/'1.sav') as after:
    for name in ('Archive_Revision','Build_Identity','Config_Identity'):
     assert before.read(name)==after.read(name),('slot refusal changed held identity',name)
    assert before.read('CPU')[:cpu_prefix]==after.read('CPU')[:cpu_prefix]
    assert before.read('Memory')[memory_descriptor:]==after.read('Memory')[memory_descriptor:]
    for name in before.namelist():
     # Mapper input can change Keyboard, not cycles/PIC/timer/other owners.
     if name not in ('Time_Stamp','Keyboard'):
      assert before.read(name)==after.read(name),('valid slot refusal changed owner',name)
   shutil.copyfile(d/'1.sav',d/'refusal-before.sav')
   # Corrupt identity with valid ZIP CRC: slot ingress must refuse, no dialog.
   bad=d/'bad.sav'
   with zipfile.ZipFile(d/'1.sav') as original, zipfile.ZipFile(bad,'w') as changed:
    for name in original.namelist(): changed.writestr(name,b'bad-revision' if name=='Archive_Revision' else original.read(name))
   shutil.copyfile(bad,d/'1.sav')
   previous_refusals=(d/'pty.log').read_bytes().count(b'State load refused: fresh launch required')
   sendkey(':'+display,'l');time.sleep(2);drain()
   assert (d/'pty.log').read_bytes().count(b'State load refused: fresh launch required')>previous_refusals
   sendkey(':'+display,'s');time.sleep(2);drain()
   deadline=time.monotonic()+15
   while time.monotonic()<deadline:
    try:
     with zipfile.ZipFile(d/'1.sav') as saved:
      if saved.read('Archive_Revision')!=b'bad-revision': break
    except (zipfile.BadZipFile,KeyError): pass
    time.sleep(.1);drain()
   with zipfile.ZipFile(d/'refusal-before.sav') as before, zipfile.ZipFile(d/'1.sav') as after:
    for name in before.namelist():
     # Mapper key/focus presentation legitimately changes keyboard input state.
     if name not in ('Time_Stamp','Keyboard'): assert before.read(name)==after.read(name),('slot refusal changed owner',name)
   return
  if file_state or os.environ.get('DOSBOX_TEST_CAPTURE_IP'):
   checkpoint=os.environ.get('DOSBOX_TEST_CAPTURE_IP','0814:0180')
   request('checkpoint-bp','BP '+checkpoint)
   request('setup-run','RUN')
   time.sleep(1);drain()
  if os.environ.get('DOSBOX_TEST_LFN_HELD'):
   # Real LFN handle allocation succeeded before IP0200. Capture must not
   # publish an archive pretending that the global native handle is classic.
   destination=w/'unrepresented-lfn.sav'
   request('lfn-save',f'STATEWRITE {destination}',expect_failure=True)
   assert not destination.exists()
   step=request('lfn-refusal-step','STEPIN')
   segment=os.environ['DOSBOX_TEST_CAPTURE_IP'].split(':')[0]
   assert ('CS:IP='+segment+':0201').encode() in step,step
   return
  if not slot_mode:
   unknown=request('unknown-command','NOT_A_DEBUGGER_COMMAND',expect_failure=True)
   assert b'Debugger command not recognized' in unknown,unknown
   missing=request('missing-argument','STATEWRITE',expect_failure=True)
   assert b'State path required' in missing,missing
   assert b'Debugger command not recognized' not in missing,missing
   request('reg','SR AX 1234')
   # Permit capture after an explicit guest setup prefix (for example setting
   # PSP parent=self). Save observations begin only after these authorized steps.
   for index in range(int(os.environ.get('DOSBOX_TEST_CAPTURE_STEPS','0'))):
    request('capture-prefix'+str(index),'STEPIN')
   request('first',f'STATEWRITE {w}/first.sav')
   time.sleep(.4)
   request('second',f'STATEWRITE {w}/second.sav')
   if flagged:
    (w/'FLAG.TXT').write_bytes(b'changed between processes\r\n')
   # This tests only the supported used-target refusal, not startup restore.
   request('invalid-destination',f'STATEWRITE {w}/absent-directory/state.sav',expect_failure=True)
   request('same-load',f'STATEREAD {w}/first.sav',expect_failure=True)
   request('missing-load',f'STATEREAD {w}/absent.sav',expect_failure=True)
   request('refusal-save',f'STATEWRITE {w}/after-live-refusal.sav')
   with zipfile.ZipFile(w/'first.sav') as before, zipfile.ZipFile(w/'after-live-refusal.sav') as after:
    for name in before.namelist():
     if name != 'Time_Stamp': assert before.read(name)==after.read(name),name
   step=request('refusal-step','STEPIN')
   expected=os.environ.get('DOSBOX_TEST_REFUSAL_REGISTERS','CS:IP=0814:0101 AX=1234 BX=0000').encode()
   if file_state:
    assert b'CS:IP=0814:0184' in step,step
   else:
    assert expected in step,step
 finally:
  if conn:conn.close()
  if p.poll() is None:p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  drain();os.close(master);log.close();wire.close();listener.close();x.terminate();x.wait(timeout=5);xlog.close()
run()
if os.environ.get('DOSBOX_TEST_LFN_HELD'):
 print('real LFN search capture refusal/no publication/explicit step probe completed',flush=True)
 raise SystemExit(0)
if unsupported_devices:
 print('nonempty devices capture refusal/no publication/explicit step probe completed',flush=True)
 raise SystemExit(0)
if slot_mode:
 print('slot mapper probe completed',flush=True)
else:
 # Stopped producer: host delay and refused loads do not advance any owner.
 with zipfile.ZipFile(w/'first.sav') as first, zipfile.ZipFile(w/'second.sav') as second:
  for name in first.namelist():
   if name != 'Time_Stamp': assert first.read(name) == second.read(name), name
 print('used-target load refusal/repeated stopped save/explicit step probe completed',flush=True)

"""Original-engine stopped save vertical (author integration probe)."""
import os, pathlib, tempfile, subprocess, pty, fcntl, termios, struct, socket, select, time, zipfile, re, json, hashlib
root = pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'stopped-save-vertical'
root.mkdir(exist_ok=True)
w = pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root))
binary = pathlib.Path(os.environ.get('DOSBOX_TEST_BINARY','src/dosbox-x')).resolve()
(w/'TINY.COM').write_bytes(bytes.fromhex('90 40 eb fc'))
flagged = bool(os.environ.get('DOSBOX_TEST_FLAGGED'))
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
def run(restore):
 d=w/('fatal' if restore == 'fatal' else 'restore' if restore else 'capture');d.mkdir()
 listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(20)
 flag_command = 'flagsave FLAG.TXT\n' if flagged else ''
 conf=d/'fixture.conf';conf.write_text(f'[dosbox]\nmcp_server={listener.getsockname()[1]}\n[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c {w}\nc:\n{flag_command}debugbox TINY.COM\n')
 xr,xw=os.pipe();xlog=open(d/'xvfb.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(xw),'-screen','0','1024x768x24'],pass_fds=(xw,),stdout=xlog,stderr=xlog);os.close(xw)
 with os.fdopen(xr) as pipe: display=pipe.readline().strip()
 master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(master,False)
 log=open(d/'pty.log','wb');wire=open(d/'wire.log','wb')
 argv=[str(binary),'-conf',str(conf)]
 if os.environ.get('DOSBOX_SAVE_BACKTRACE'):
  argv=['gdb','-batch','-ex','break '+os.environ['DOSBOX_SAVE_BACKTRACE'],'-ex','run','-ex','bt','-ex','continue','--args']+argv
 p=subprocess.Popen(argv,stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy'));os.close(slave)
 conn=None;buf=b''
 def drain():
  while select.select([master],[],[],0)[0]:
   try: data=os.read(master,65536)
   except OSError: break
   if not data:break
   log.write(data);log.flush()
 def request(ident,command,expect_failure=False):
  nonlocal buf
  text=f'REQ {ident} EXEC {command}\n'.encode();conn.sendall(text);wire.write(text);wire.flush()
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
  assert out.startswith(expected), out
  if command.startswith(('STATEWRITE ', 'STATEREAD ')):
   assert b'Debugger command not recognized' not in out, out
   assert (b'State failed' if expect_failure else b'State completed') in out, out
   assert (b'State completed' if expect_failure else b'State failed') not in out, out
  return out
 try:
  conn,_=listener.accept();conn.setblocking(False)
  time.sleep(2);drain()
  if restore == 'fatal':
   # Valid transport, invalid consumed owner: preceding owners may have effects.
   bad = w/'short-pic.sav'
   with zipfile.ZipFile(w/'first.sav') as original, zipfile.ZipFile(bad, 'w') as changed:
    for name in original.namelist():
     changed.writestr(name, b'' if name == 'Pic' else original.read(name))
   command = f'REQ fatal EXEC STATEREAD {bad}\n'.encode()
   conn.sendall(command);wire.write(command);wire.flush()
   end=time.monotonic()+20
   eof=False
   while time.monotonic()<end:
    drain()
    if select.select([conn],[],[],.02)[0]:
     data=conn.recv(65536)
     if not data:
      eof=True
      break
     wire.write(data);wire.flush();buf+=data
   assert eof, 'partial install left the frontend connection alive'
   rc=p.wait(timeout=5)
   (d/'exit-code.txt').write_text(str(rc)+'\n')
   assert rc == 1, ('fatal disposal did not exit through E_Exit', rc)
   drain()
   fatal_log = (d/'pty.log').read_bytes()
   assert b'Invalid or truncated saved PIC queue link' in fatal_log
   assert b'terminate called' not in fatal_log, fatal_log[-2000:]
   assert b'State completed' not in buf and b'END fatal' not in buf, buf
   return
  if restore:
   request('ready','SR BX 5678')
   unknown = request('unknown-command','NOT_A_DEBUGGER_COMMAND',expect_failure=True)
   assert b'Debugger command not recognized' in unknown, unknown
   missing_argument = request('missing-argument','STATEWRITE',expect_failure=True)
   assert b'State path required' in missing_argument, missing_argument
   assert b'Debugger command not recognized' not in missing_argument, missing_argument
   request('before-refusals',f'STATEWRITE {w}/before-refusals.sav')
   request('missing-path',f'STATEREAD {w}/absent.sav',expect_failure=True)
   request('invalid-destination',f'STATEWRITE {w}/absent-directory/state.sav',expect_failure=True)
   for field in ('DOSBox-X_Version', 'Program_Name', 'Memory_Size', 'Machine_Type'):
    bad = w / (field + '.sav')
    with zipfile.ZipFile(w/'first.sav') as original, zipfile.ZipFile(bad, 'w') as changed:
     for name in original.namelist():
      # Numeric mismatch used to overflow diagnostic-only atoi/multiplication.
      mismatch = b'2147483647' if field == 'Memory_Size' else b'incompatible'
      changed.writestr(name, mismatch if name == field else original.read(name))
    refused = request('mismatch',f'STATEREAD {bad}',expect_failure=True)
    assert b'State failed' in refused and b'State completed' not in refused, refused
   # Real ZIP integrity refusal must happen before the first owner install.
   with zipfile.ZipFile(w/'first.sav') as original:
    owner = original.namelist()[-1]
    for kind in ('missing', 'crc'):
     bad = w / (kind + '.sav')
     with zipfile.ZipFile(bad, 'w', compression=zipfile.ZIP_STORED) as changed:
      for name in original.namelist():
       if kind != 'missing' or name != owner:
        changed.writestr(name, original.read(name))
     if kind == 'crc':
      with zipfile.ZipFile(bad) as changed:
       entry = changed.getinfo(owner)
      data = bytearray(bad.read_bytes())
      name_length, extra_length = struct.unpack_from('<HH', data, entry.header_offset + 26)
      offset = entry.header_offset + 30 + name_length + extra_length
      assert entry.file_size > 0
      data[offset] ^= 1
      bad.write_bytes(data)
     refused = request(kind, f'STATEREAD {bad}', expect_failure=True)
     assert b'State failed' in refused and b'State completed' not in refused, refused
   request('after-refusals',f'STATEWRITE {w}/after-refusals.sav')
   request('load',f'STATEREAD {w}/first.sav')
   if flagged:
    assert (w/'FLAG.TXT').read_bytes() == b'captured flagged guest file\r\n'
   request('again',f'STATEWRITE {w}/restored.sav')
   time.sleep(.4)
   request('held',f'STATEWRITE {w}/restored-held.sav')
  else:
   request('reg','SR AX 1234')
   request('first',f'STATEWRITE {w}/first.sav')
   time.sleep(.4)
   request('second',f'STATEWRITE {w}/second.sav')
   request('same-load',f'STATEREAD {w}/first.sav')
   request('same-again',f'STATEWRITE {w}/same-restored.sav')
   time.sleep(.4)
   request('same-held',f'STATEWRITE {w}/same-restored-held.sav')
  step = request('step','STEPIN')
  assert b'CS:IP=0814:0101 AX=1234 BX=0000' in step, step
 finally:
  if conn:conn.close()
  if p.poll() is None:p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  drain();os.close(master);log.close();wire.close();listener.close();x.terminate();x.wait(timeout=5);xlog.close()
run(False)
if flagged:
 (w/'FLAG.TXT').write_bytes(b'changed between processes\r\n')
run(True)
run('fatal')
# Same-process host delay must not change any owner payload. Fresh-process
# archives contain process-local addresses in legacy owners; do not mistake
# whole-archive byte equality for their still-required semantic qualification.
with zipfile.ZipFile(w/'before-refusals.sav') as before, zipfile.ZipFile(w/'after-refusals.sav') as after:
 for name in before.namelist():
  if name != 'Time_Stamp': assert before.read(name) == after.read(name), ('refusal changed owner', name)
with zipfile.ZipFile(w/'first.sav') as first, zipfile.ZipFile(w/'second.sav') as second:
 for name in first.namelist():
  if name != 'Time_Stamp': assert first.read(name) == second.read(name), name
with zipfile.ZipFile(w/'same-restored.sav') as restored, zipfile.ZipFile(w/'same-restored-held.sav') as held, zipfile.ZipFile(w/'first.sav') as first:
 for name in restored.namelist():
  if name != 'Time_Stamp': assert restored.read(name) == held.read(name), ('same-process restored delay', name)
 for name in ('Pic', 'IntTimer10'):
  assert first.read(name) == restored.read(name), ('same-process restored state', name)
 assert first.read('CPU')[:cpu_prefix] == restored.read('CPU')[:cpu_prefix], 'same-process CPU architectural/cycle state'
 assert first.read('Memory')[memory_descriptor:] == restored.read('Memory')[memory_descriptor:], 'same-process guest RAM/handles/page identities'
with zipfile.ZipFile(w/'restored.sav') as restored, zipfile.ZipFile(w/'restored-held.sav') as held, zipfile.ZipFile(w/'first.sav') as first:
 for name in restored.namelist():
  if name != 'Time_Stamp': assert restored.read(name) == held.read(name), name
 for name in ('Pic', 'IntTimer10'):
  assert first.read(name) == restored.read(name), name
 assert first.read('CPU')[:cpu_prefix] == restored.read('CPU')[:cpu_prefix], 'CPU architectural/cycle state'
 assert first.read('Memory')[memory_descriptor:] == restored.read('Memory')[memory_descriptor:], 'guest RAM/handles/page identities'
(w/'receipt.json').write_text(json.dumps({'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(), 'cpu_prefix':cpu_prefix, 'memory_descriptor':memory_descriptor, 'flagged_file_restore':flagged, 'probe_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(), 'checks':['valid-CRC truncated Pic causes connection EOF, E_Exit and nonzero process disposal (exit cleanup quality unqualified)','missing source and invalid destination return ERR without modal UI','four explicit metadata and missing/CRC-corrupt owner refusals without changed owner payloads','repeated completed saves','fresh stopped restore/re-save','host delay payload equality before/after restore','fresh CPU architectural/cycle prefix equality','fresh guest RAM/handle/page identity equality','fresh Pic/IntTimer10 equality','explicit one-step CS:IP/AX/BX']},indent=2))
print('stopped save and consumed-owner fatal-disposal probe completed (exit cleanup/device/resource qualification remains)',flush=True)

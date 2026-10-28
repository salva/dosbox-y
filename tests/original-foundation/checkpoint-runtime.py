#!/usr/bin/env python3
"""Real production live checkpoint consumer; private process/display and bounded I/O."""
import os,pathlib,tempfile,subprocess,pty,fcntl,termios,struct,time,select
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'checkpoint-runtime'
root.mkdir(exist_ok=True)
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root))
(w/'TINY.COM').write_bytes(bytes.fromhex('40 eb fd'))
(w/'fixture.conf').write_text(f'[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[dos]\nlfn=false\n[autoexec]\nmount c {w}\nc:\nTINY.COM\n')
(w/'dispatcher.lua').write_text('''return {
live_dispatch=function(line) local f,e=load(line,'=fixture'); if not f then return 'ERROR '..e end; local ok,r=pcall(f); return (ok and '' or 'ERROR ')..tostring(r) end,
live_finish=function(result) return result end
}
''')
xr,xw=os.pipe();xl=open(w/'xvfb.log','wb')
x=subprocess.Popen(['Xvfb','-displayfd',str(xw),'-screen','0','1024x768x24'],pass_fds=(xw,),stdout=xl,stderr=xl);os.close(xw)
with os.fdopen(xr) as f: display=f.readline().strip()
assert display
ri,wi=os.pipe2(os.O_NONBLOCK);ro,wo=os.pipe2(os.O_NONBLOCK)
master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(master,False)
env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy',DOSBOX_Y_LUA_INPUT_FD=str(ri),DOSBOX_Y_LUA_OUTPUT_FD=str(wo),DOSBOX_Y_LUA_LIVE_MODULE=str(w/'dispatcher.lua'),DOSBOX_Y_LUA_LAUNCH_HOLD_MS='10000')
p=subprocess.Popen([os.environ['DOSBOX_TEST_BINARY'],'-conf',str(w/'fixture.conf')],stdin=slave,stdout=slave,stderr=slave,env=env,pass_fds=(ri,wo),cwd=w)
os.close(slave);os.close(ri);os.close(wo)
log=open(w/'pty.log','wb');wire=open(w/'wire.log','wb');buf=b''
def drain():
 if select.select([master],[],[],0)[0]:
  try: log.write(os.read(master,65536));log.flush()
  except OSError: pass

def call(code):
 global buf
 wire.write(b'> '+code.encode()+b'\n');wire.flush();os.write(wi,code.encode()+b'\n')
 end=time.monotonic()+15
 while b'\n' not in buf:
  drain()
  if p.poll() is not None: raise AssertionError(('emulator exited',p.returncode,w))
  assert time.monotonic()<end,('reply timeout',w)
  if select.select([ro],[],[],.02)[0]: buf+=os.read(ro,65536)
 result,buf=buf.split(b'\n',1);wire.write(result+b'\n');wire.flush();print(result.decode(),flush=True)
 return result.decode()
try:
 print(w,flush=True)
 assert call("local r=dbg.live.checkpoint{op='save',id='root'}; assert(not r.ok); return r.error")=='requires deliberate suspended EXEC stop'
 assert call("local r=dbg.live.executable_entry{op='arm',program='C:\\\\TINY.COM',wall_ms=10000}; dbg.live.resume(); return r.reason")=='armed'
 # Native frame is now the naturally entered EXEC child, not startup/root.
 end=time.monotonic()+8
 while True:
  value=call('local s=dbg.live.status(); return s.owner_kind')
  if value=='ShellExecGuest':break
  assert time.monotonic()<end
  time.sleep(.05)
 assert call('dbg.live.stop(); return true')=='true'
 assert call("local r=dbg.live.checkpoint{op='load',id='missing'}; assert(not r.ok); return r.error")=='checkpoint owner/PSP/mount mismatch or missing'
 assert call("local r=dbg.live.checkpoint{op='save',id='a'}; assert(r.ok,r.error); return r.ok")=='true'
 saved=call("return dbg.reg('AX')")
 assert call("dbg.live.phase{instruction_limit=2,wall_ms=100}; return true")=='true'
 changed=call("return dbg.reg('AX')")
 assert changed!=saved,(saved,changed)
 assert call("local r=dbg.live.checkpoint{op='save',id='b'}; assert(r.ok,r.error); return r.ok")=='true'
 for i in range(19):
  assert call("local r=dbg.live.checkpoint{op='load',id='a'}; assert(r.ok,r.error); assert(r.state_epoch=="+str(i+1)+"); return dbg.reg('AX')")==saved
 assert call("local r=dbg.live.checkpoint{op='load',id='b'}; assert(r.ok,r.error); return dbg.reg('AX')")==changed
 assert call("assert(dbg.live.checkpoint{op='save',id='a'}.ok); assert(dbg.live.checkpoint{op='load',id='a'}.ok); return dbg.reg('AX')")==changed
 before=call("return dbg.reg('IP')")
 time.sleep(.3)
 assert call("return dbg.reg('IP')")==before
 # Only explicit phase enters decoder; installation itself remains stopped.
 after=call("dbg.live.phase{instruction_limit=2,wall_ms=100}; return dbg.reg('AX')")
 assert after!=changed,(after,changed)
 print('production checkpoint repeated/replacement/held/explicit-continuation PASS',flush=True)
finally:
 if p.poll() is None:p.terminate()
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 drain();x.terminate();x.wait(timeout=5)
 for fd in (wi,ro,master):os.close(fd)
 log.close();wire.close();xl.close()

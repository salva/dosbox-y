import os,pty,fcntl,termios,struct,subprocess,socket,time,pathlib,select,hashlib
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'exclusive-current-probe-loop';w.mkdir(exist_ok=True)
# Keep receipts from each invocation instead of overwriting failed witnesses.
import tempfile
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=w))
binary=pathlib.Path(os.environ.get('DOSBOX_TEST_BINARY','src/dosbox-x')).resolve()
def run(bridge, no_lua=False, lua=False):
 d=w/('bridge' if bridge else 'interactive');d.mkdir(exist_ok=True)
 listener=None;conn=None
 if bridge:
  listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(10)
 conf=d/'fixture.conf';conf.write_text(('[dosbox]\nmcp_server='+str(listener.getsockname()[1])+'\n' if bridge else '')+'[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 xr,xw=os.pipe();xlog=open(d/'xvfb.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(xw),'-screen','0','1024x768x24'],pass_fds=(xw,),stdout=xlog,stderr=xlog);os.close(xw)
 with os.fdopen(xr) as display_pipe: display=display_pipe.readline().strip()
 assert display and x.poll() is None, 'private Xvfb failed before display readiness'
 master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(master,False)
 env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy')
 p=subprocess.Popen([str(binary),'-conf',str(conf),'-break-start','-noautoexec'],stdin=slave,stdout=slave,stderr=slave,env=env);os.close(slave)
 log=open(d/'pty.log','wb');wire=open(d/'wire.log','wb');buf=b''
 def drain(seconds):
  end=time.monotonic()+seconds
  while time.monotonic()<end:
   if select.select([master],[],[],.05)[0]:
    try:data=os.read(master,65536)
    except OSError:break
    log.write(data);log.flush()
 def request(text):
  nonlocal buf
  conn.sendall(text.encode()+b'\n');wire.write(b'> '+text.encode()+b'\n');wire.flush();ident=text.split()[1];end=time.monotonic()+10
  while ('END '+ident+'\n').encode() not in buf and time.monotonic()<end:
   drain(.05)
   if select.select([conn],[],[],.05)[0]:
    data=conn.recv(65536)
    if not data:raise RuntimeError('EOF')
    buf+=data
  marker=('END '+ident+'\n').encode();assert marker in buf,buf
  result,buf=buf.split(marker,1);result+=marker;wire.write(result);wire.flush();print(result.decode(),flush=True);return result
 try:
  drain(2);assert p.poll() is None
  if bridge:
   conn,_=listener.accept();conn.setblocking(False);drain(.5)
   assert b'OK' in request('REQ p PING')
   assert b'E05B' in request('REQ s1 EXEC STEPIN')
   assert b'C9B0' in request('REQ s2 EXEC STEPIN')
   assert b'OK' in request('REQ bp EXEC BP F000:C9B0')
   assert b'OK' in request('REQ reg EXEC SR AX 1234')
   os.write(master,b'SR AX 5678\n');drain(.5);os.write(master,b'\x1b[20~');drain(.5)
   assert b'breakpoint admission refused' in (d/'pty.log').read_bytes()
   assert b'F000:C9B0' in request('REQ inventory EXEC BPLIST')
   assert b'Breakpoint on SYSENTER set' in request('REQ sysenter_old EXEC BPSYSENTER')
   assert b'Breakpoint on SYSEXIT set' in request('REQ sysexit_old EXEC BPSYSEXIT')
   conn.close();conn=None;drain(1.5)
   conn,_=listener.accept();conn.setblocking(False);buf=b'';drain(.5)
   again=request('REQ again EXEC STEPIN')
   assert b'OK' in again and b'AX=1234' in again
   assert b'F000:C9B0' not in request('REQ clean EXEC BPLIST')
   # Distinct IDs: the replacement's first toggles must set, not clear old flags.
   assert b'Breakpoint on SYSENTER set' in request('REQ sysenter_new EXEC BPSYSENTER')
   assert b'Breakpoint on SYSEXIT set' in request('REQ sysexit_new EXEC BPSYSEXIT')
   # Use a two-byte loop rather than single-stepping an unbounded BIOS callback.
   request('REQ handoffmem EXEC SM 1000:0 EB FE')
   request('REQ handoffcs EXEC SR CS 1000')
   request('REQ handoffip EXEC SR EIP 0')
   # Return to Interactive only after an actual bridge socket departure. Then
   # explicit Interactive detach permits a fresh Bridge in this same process.
   # Keep reconnect from racing terminal attachment. Connect logs only after
   # native departure retirement, so it is an actual lifecycle witness.
   address=listener.getsockname();listener.close();listener=None
   departures=(d/'pty.log').read_bytes().count(b'[ControlClient] connecting to')
   conn.close();conn=None
   deadline=time.monotonic()+10
   while (d/'pty.log').read_bytes().count(b'[ControlClient] connecting to')<=departures:
    assert time.monotonic()<deadline, 'bridge departure not retired'
    drain(.05)
   successes=(d/'pty.log').read_bytes().count(b'Set Register success.')
   os.write(master,b'SR AX 5678\n')
   deadline=time.monotonic()+10
   while (d/'pty.log').read_bytes().count(b'Set Register success.')<=successes:
    assert time.monotonic()<deadline, 'interactive attachment not acquired'
    drain(.05)
   os.write(master,b'STEPIN\n');drain(.5)
   listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
   listener.bind(address);listener.listen();listener.settimeout(10)
   conn,_=listener.accept();conn.setblocking(False);buf=b'';drain(.3)
   assert b'BEGIN competing ERR' in request('REQ competing EXEC STEPIN')
   os.write(master,b'DETACH\n');drain(.5)
   assert b'session detach complete' in (d/'pty.log').read_bytes()
   assert b'BEGIN reused OK' in request('REQ reused EXEC STEPIN')
   request('REQ loop EXEC SM 1000:0 EB FE')
   request('REQ cs EXEC SR CS 1000')
   request('REQ ip EXEC SR EIP 0')
   request('REQ run EXEC RUN');drain(2)
   # A premature budget terminal must not be hidden by a later BREAK reply.
   assert not select.select([conn],[],[],0)[0], 'RUN ended before explicit BREAK'
   assert b'OK' in request('REQ stop BREAK')
   final=request('REQ final EXEC STEPIN')
   assert b'BEGIN final OK\n' in final and b'CS:IP=1000:0000' in final,final
   # Actual socket loss while Active: wait for unwind before replacement.
   request('REQ lost EXEC RUN');drain(.3)
   conn.close();conn=None;drain(1.5)
   conn,_=listener.accept();conn.setblocking(False);buf=b'';drain(.5)
   recovered=request('REQ recovered EXEC STEPIN')
   assert b'BEGIN recovered OK\n' in recovered and b'CS:IP=1000:0000' in recovered,recovered
   assert b'lost' not in recovered
   # A raw selected terminate may close before replying. Never replay it.
   conn.sendall(b'REQ quit EXEC QUIT\n');drain(1)
   assert p.wait(timeout=5)==0
  else:
   if lua:
    def command(text, expected):
     before=(d/'pty.log').read_bytes()
     os.write(master,text.encode()+b'\n');drain(.5)
     added=(d/'pty.log').read_bytes()[len(before):]
     assert expected in added,(text,added[-2500:])
    command('LUA persistent=40',b'')
    command('LUA persistent+2',b'42')
    command('LUA print("sink-marker"); dbg.log("log-marker")',b'log-marker')
    command('LUA error("repair-marker")',b'repair-marker')
    command('LUA persistent+3',b'43')
    # Exercise the production persistent evaluator, not an extracted mock.
    module=d/'interactive-module.lua';module.write_text('return {value=73}\n')
    script=d/'interactive-file.lua';script.write_text('file_value=91; print("file-marker")\n')
    command('LUA assert(io and os and package and debug and coroutine); package.path="'+str(d)+'/?.lua;"..package.path; require("interactive-module").value',b'73')
    command('LUAFILE '+str(script),b'file-marker')
    command('LUA file_value',b'91')
    command('LUA coroutine.wrap(function() print("coroutine-marker") end)()',b'coroutine-marker')
    command('LUA dbg.step()',b'Interactive')
    command('LUA persistent+4',b'44')
    command('LUA final=setmetatable({}, {__gc=function() print("final-marker") end})',b'')
    command('LUARESET',b'final-marker')
    command('LUA persistent==nil',b'true')
   if no_lua:
    for command in (b'LUA 1+2\n',b'LUAFILE missing.lua\n',b'LUARESET\n'):
     os.write(master,command);drain(.4)
    assert (d/'pty.log').read_bytes().count(b'Lua support is not compiled in')>=3
   os.write(master,b'STEPIN\n');drain(1);os.write(master,b'STEPIN\n');drain(1)
   terminal=(d/'pty.log').read_bytes()
   assert b'E05B' in terminal and b'C9B0' in terminal
 finally:
  if conn:conn.close()
  if listener:listener.close()
  p.terminate()
  try:rc=p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();rc=p.wait()
  drain(.1);os.close(master);log.close();wire.close();x.terminate();xrc=x.wait(timeout=5);xlog.close()
  (d/'identity.txt').write_text(f'binary={binary}\nsha256={hashlib.sha256(binary.read_bytes()).hexdigest()}\nemulator_rc={rc}\nxvfb_rc={xrc}\n')
if __name__ == '__main__':
 run(False);run(True)

"""Real Agent outstanding Over/Pause regression. No-id Step intentionally uses
existing timeout behavior without entering the unrelated deferred-request gate.
Startup/PTY/cleanup follows rpc-runtime.py; test-only, no product protocol change.
"""
import os, pty, fcntl, termios, struct, subprocess, socket, time, pathlib, select, json, tempfile
base=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'agent-over-pause'
base.mkdir(exist_ok=True)
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=base))
(w/'HANG.COM').write_bytes(bytes.fromhex('e8 02 00 90 90 eb fe'))
(w/'fixture.conf').write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
binary=pathlib.Path(os.environ['DOSBOX_TEST_BINARY']).resolve()
(w/'agent.env').write_text(f'transport=unix_socket\nendpoint={w}/rpc.sock\ndosbox_executable={binary}\ndosbox_workdir={w}\nprofile=test\nrequest_timeout_ms=5000\nmax_message_bytes=1048576\nmax_memory_read_bytes=65536\nmax_trace_events=10000\n')
r,t=os.pipe(); xl=open(w/'x.log','wb')
x=subprocess.Popen(['Xvfb','-displayfd',str(t),'-screen','0','1024x768x24'],pass_fds=(t,),stdout=xl,stderr=xl);os.close(t)
with os.fdopen(r) as f: display=f.readline().strip()
m,s=pty.openpty();fcntl.ioctl(s,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(m,False)
argv=[str(binary),'-conf',str(w/'fixture.conf'),'-agent-config',str(w/'agent.env'),'-noautoexec']
if os.environ.get('AGENT_PAUSE_SELECTION_PROBE'):
 # Controlled delayed-selection scenario on the actual queued Agent consumer:
 # replace the session's current binding with empty after ingress captured its
 # producer. This controlled successor-slot independence probe is not a second
 # guest operation; native exact successor rejection is covered by control.cpp.
 source=pathlib.Path('src/agent/server/agent_server.cpp').resolve()
 lines=source.read_text().splitlines()
 before=next(i+1 for i,l in enumerate(lines) if 'if(execution_binding)' in l)
 after=next(i+1 for i,l in enumerate(lines) if 'operations.at(operation_id).native_operation=native_operation' in l)
 script=w/'selection.gdb'
 script.write_text(f'''set pagination off
set confirm off
break {source}:{before}
commands
silent
printf "selected producer before current-slot replacement\\n"
print *execution_binding._M_ptr
set impl->session->execution_binding._M_ptr = 0
continue
end
break {source}:{after}
commands
silent
printf "selected native after current-slot replacement\\n"
print native_operation
continue
end
run
''')
 argv=['gdb','-q','-batch','-x',str(script),'--args']+argv
p=subprocess.Popen(argv,stdin=s,stdout=s,stderr=s,env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy'));os.close(s)
log=open(w/'pty.log','wb');c=None
print(w,flush=True)
def drain(sec):
 end=time.monotonic()+sec
 while time.monotonic()<end:
  if select.select([m],[],[],.02)[0]:
   try: log.write(os.read(m,65536));log.flush()
   except OSError: break
try:
 drain(10);assert p.poll() is None
 c=socket.socket(socket.AF_UNIX);c.connect(str(w/'rpc.sock'));c.setblocking(False)
 buf=b''
 def req(method,params,notification=False):
  global buf
  request=dict(jsonrpc='2.0',method=method,params=params)
  if not notification: request['id']=str(time.monotonic_ns())
  c.sendall(json.dumps(request).encode()+b'\n');print('>',request,flush=True)
  end=time.monotonic()+15
  while b'\n' not in buf and time.monotonic()<end:
   drain(.02)
   if select.select([c],[],[],.02)[0]:
    data=c.recv(65536);assert data;buf+=data
  assert b'\n' in buf,buf
  line,buf=buf.split(b'\n',1);result=json.loads(line);print('<',result,flush=True);return result
 start=req('session.start',dict(target=dict(command='HANG.COM',arguments=[]),mounts=[dict(drive='C',host_path=str(w))],break_at='entry'))
 sid=start['result']['session_id'];params=dict(session_id=sid)
 def pause():
  assert req('session.status',params)['result']['state']=='running'
  op=req('execution.pause',params)['result']['operation_id']
  stopped=req('execution.wait',dict(**params,operation_id=op,timeout_ms=1000))
  assert stopped['result']['state']=='stopped',stopped
  assert stopped['result']['stop_reason']['kind']=='pause',stopped
 def over():
  timeout=req('execution.step',dict(**params,mode='over'),True)
  assert timeout['error']['data']['reason']=='OPERATION_TIMEOUT',timeout
  pause()
 over() # first producer is Step, no Continue binding exists
 # Populate a Continue binding, then replace the stopped callee with another
 # CALL/infinite callee. Over must select itself, not that old Continue.
 req('execution.continue',params)
 pause()
 regs=req('state.get_registers',params)['result']
 import base64
 code=bytes.fromhex('e8 02 00 90 90 eb fe')
 written=req('memory.write',dict(**params,address=dict(space='segmented',segment=regs['segments']['cs'],offset=regs['instruction_pointer']),data_base64=base64.b64encode(code).decode()))
 assert 'result' in written,written
 over()
 req('session.stop',params)
 if os.environ.get('AGENT_PAUSE_SELECTION_PROBE'):
  drain(.2)
  transcript=(w/'pty.log').read_text(errors='replace')
  assert transcript.count('selected producer before current-slot replacement')==3,transcript
  assert transcript.count('selected native after current-slot replacement')==3,transcript
  assert 'Error in sourced command file' not in transcript,transcript
 print('Outstanding first Over and Over-after-Continue Pause PASS',flush=True)
finally:
 if c:c.close()
 p.terminate()
 try:p.wait(timeout=10)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 x.terminate();x.wait(timeout=10);drain(.1);log.close();os.close(m);xl.close()
 print('cleanup',p.returncode,x.returncode,flush=True)

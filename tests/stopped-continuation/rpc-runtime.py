import os,pty,fcntl,termios,struct,subprocess,socket,time,pathlib,select,json,tempfile
base=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'rpc-runtime';base.mkdir(exist_ok=True)
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=base))
(w/'TINY.COM').write_bytes(bytes.fromhex('90 90 e8 05 00 b8 00 4c cd 21 90 c3'))
(w/'CHILD.COM').write_bytes(bytes.fromhex('b8 00 4c cd 21'))
# INT21/31 resident termination is not proof that the selected target is absent.
(w/'STAY.COM').write_bytes(bytes.fromhex('ba 20 00 b8 00 31 cd 21'))
(w/'LOOP.COM').write_bytes(bytes.fromhex('eb fe'))
subprocess.run(['nasm','-f','bin','tests/stopped-continuation/rpc-nested.asm','-o',str(w/'NEST.COM')],check=True)
(w/'fixture.conf').write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
binary=pathlib.Path(os.environ.get('DOSBOX_TEST_BINARY','src/dosbox-x')).resolve()
(w/'agent.env').write_text(f'transport=unix_socket\nendpoint={w}/rpc.sock\ndosbox_executable={binary}\ndosbox_workdir={w}\nprofile=test\nrequest_timeout_ms=5000\nmax_message_bytes=1048576\nmax_memory_read_bytes=65536\nmax_trace_events=10000\n')
r,t=os.pipe();xl=open(w/'x.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(t),'-screen','0','1024x768x24'],pass_fds=(t,),stdout=xl,stderr=xl);os.close(t)
with os.fdopen(r) as f: display=f.readline().strip()
m,s=pty.openpty();fcntl.ioctl(s,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(m,False)
p=subprocess.Popen(['gdb','-batch','-ex','run','-ex','thread apply all bt','--args',str(binary),'-conf',str(w/'fixture.conf'),'-agent-config',str(w/'agent.env'),'-noautoexec'],stdin=s,stdout=s,stderr=s,env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy'));os.close(s)
log=open(w/'pty.log','wb');c=None
print(w,flush=True)
def drain(sec):
 end=time.monotonic()+sec
 while time.monotonic()<end:
  if select.select([m],[],[],.02)[0]:
   try:log.write(os.read(m,65536));log.flush()
   except OSError:break
try:
 drain(10);assert p.poll() is None
 c=socket.socket(socket.AF_UNIX);c.connect(str(w/'rpc.sock'));c.setblocking(False)
 def req(method,params={},id=None):
  if id is None:id=str(time.monotonic_ns())
  data=json.dumps(dict(jsonrpc='2.0',id=id,method=method,params=params)).encode()+b'\n';c.sendall(data);print('>',data.decode().strip(),flush=True)
  buf=b'';end=time.monotonic()+12
  while b'\n' not in buf and time.monotonic()<end:
   drain(.03)
   if select.select([c],[],[],.03)[0]:
    b=c.recv(65536);assert b,'EOF';buf+=b
  assert b'\n' in buf,buf
  print('<',buf.decode().strip(),flush=True);return json.loads(buf)
 start=req('session.start',{'target':{'command':'TINY.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'start')
 assert 'result' in start,start
 status=req('session.status',{'session_id':'ses-1'})
 assert 'result' in status,status
 initial=req('state.get_registers',{'session_id':'ses-1'})
 assert initial['result']['instruction_pointer']=='0x00000100',initial
 def read_code(sid,seg,offset,length):
  import base64
  value=req('memory.read',{'session_id':sid,'address':{'space':'segmented','segment':seg,'offset':offset},'length':length})
  assert 'result' in value,value
  return base64.b64decode(value['result']['data_base64'])
 code_segment=initial['result']['segments']['cs']
 code=read_code('ses-1',code_segment,'0x00000100',12)
 assert code==bytes.fromhex('90 90 e8 05 00 b8 00 4c cd 21 90 c3')
 # RPC has no native disassembly method. Decode the exact selected bytes at
 # the consumer, rather than inventing an additional native observation API.
 decoded=subprocess.run(['ndisasm','-b','16','-o','0x100','-'],input=code,stdout=subprocess.PIPE,check=True).stdout.decode()
 print('selected RPC code disassembly:\n'+decoded,flush=True)
 assert 'call 0x10a' in decoded and 'mov ax,0x4c00' in decoded,decoded
 step=req('execution.step',{'session_id':'ses-1','mode':'into'},'step')
 assert 'result' in step,step
 assert step['result']['state_revision']==2,step
 assert step['result']['registers']['instruction_pointer']=='0x00000101',step
 segment=step['result']['registers']['segments']['cs']
 fresh=req('state.get_registers',{'session_id':'ses-1'})
 assert fresh['result']['instruction_pointer']=='0x00000101',fresh
 assert read_code('ses-1',segment,'0x00000101',4)==bytes.fromhex('90 e8 05 00')
 # Exercise the affected write after-image without changing guest semantics.
 write=req('memory.write',{'session_id':'ses-1','address':{'space':'segmented','segment':segment,'offset':'0x00000101'},'data_base64':'kA=='})
 assert 'result' in write,write
 assert read_code('ses-1',segment,'0x00000101',1)==b'\x90'
 invalid=req('memory.read',{'session_id':'ses-1','address':{'space':'physical','offset':'0xFFFFFFFF'},'length':1})
 assert invalid.get('error',{}).get('data',{}).get('cause')=='physical_out_of_range',invalid
 assert invalid['error']['data']['failing_offset']=='0xFFFFFFFF',invalid
 assert invalid['error']['message']=='Unable to access the requested memory range',invalid
 bp=req('breakpoints.create',{'session_id':'ses-1','kind':'execution','address':{'space':'segmented','segment':segment,'offset':'0x00000102'},'once':True})
 assert 'result' in bp,bp
 held_run=req('execution.continue',{'session_id':'ses-1'},'continue-held')
 assert 'result' in held_run,held_run
 drain(.5)
 held=req('execution.wait',{'session_id':'ses-1','operation_id':held_run['result']['operation_id'],'timeout_ms':1000})
 assert held['result']['state']=='stopped',held
 assert held['result']['stop_reason']['kind']=='breakpoint',held
 fresh=req('state.get_registers',{'session_id':'ses-1'})
 assert fresh['result']['instruction_pointer']=='0x00000102',fresh
 assert read_code('ses-1',segment,'0x00000102',3)==bytes.fromhex('e8 05 00')
 # Socket loss is not target exit or permission to replay an accepted effect.
 c.close()
 c=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);c.connect(str(w/'rpc.sock'));c.setblocking(False)
 retry_held=req('execution.continue',{'session_id':'ses-1'},'continue-held')
 assert retry_held==held_run,(retry_held,held_run)
 retained=req('session.status',{'session_id':'ses-1'})
 assert retained['result']['state']=='stopped',retained
 assert retained['result']['state_revision']==held['result']['state_revision'],retained
 over=req('execution.step',{'session_id':'ses-1','mode':'over'},'over')
 assert 'result' in over,over
 assert over['result']['registers']['instruction_pointer']=='0x00000105',over
 assert over['result']['stop_reason']['kind']=='step',over
 # Cached request retry must not execute CALL/MOV again.
 retried=req('execution.step',{'session_id':'ses-1','mode':'over'},'over')
 assert retried==over,retried
 continued=req('execution.continue',{'session_id':'ses-1'},'continue')
 assert 'result' in continued,continued
 drain(2)
 final=req('session.status',{'session_id':'ses-1'})
 assert final['result']['state']=='exited',final
 assert final['result']['last_stop']['kind']=='program_exit',final
 old_wait=req('execution.wait',{'session_id':'ses-1','operation_id':held_run['result']['operation_id'],'timeout_ms':0})
 assert old_wait['result']['state']=='stopped',old_wait
 assert old_wait['result']['stop_reason']['kind']=='breakpoint',old_wait
 assert old_wait['result']['state_revision']==held['result']['state_revision'],old_wait
 waited=req('execution.wait',{'session_id':'ses-1','operation_id':continued['result']['operation_id'],'timeout_ms':1000})
 assert 'result' in waited,waited
 next_target=req('session.start',{'target':{'command':'TINY.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'start-next')
 assert 'result' in next_target,next_target
 assert next_target['result']['state']=='stopped',next_target
 stopped=req('session.stop',{'session_id':next_target['result']['session_id']})
 assert 'result' in stopped,stopped
 drain(1)
 terminated=req('session.status',{'session_id':next_target['result']['session_id']})
 assert terminated['result']['state']=='exited',terminated
 assert terminated['result']['last_stop']['kind']=='program_exit',terminated
 nested=req('session.start',{'target':{'command':'NEST.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'nested')
 assert 'result' in nested,nested
 nested_id=nested['result']['session_id']
 for offset in ('0x00000124','0x0000012B'):
  guard=req('breakpoints.create',{'session_id':nested_id,'kind':'execution','address':{'space':'segmented','segment':segment,'offset':offset},'once':True})
  assert 'result' in guard,guard
 nested_run=req('execution.continue',{'session_id':nested_id},'nested-run')
 drain(2)
 nested_status=req('session.status',{'session_id':nested_id})
 assert nested_status['result']['state']=='stopped',nested_status
 assert nested_status['result']['last_stop']['kind']!='program_exit',nested_status
 assert nested_status['result']['last_stop']['address']['offset']=='0x00000124',nested_status
 refused=req('session.start',{'target':{'command':'TINY.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'nested-refusal')
 assert 'error' in refused,refused
 nested_exit=req('execution.continue',{'session_id':nested_id},'nested-exit')
 drain(2)
 nested_done=req('session.status',{'session_id':nested_id})
 assert nested_done['result']['state']=='exited',nested_done
 stale=req('execution.continue',{'session_id':'ses-1'},'stale')
 assert 'error' in stale,stale
 looping=req('session.start',{'target':{'command':'LOOP.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'looping')
 loop_id=looping['result']['session_id']
 before=req('state.get_registers',{'session_id':loop_id})['result']
 for conditions in ([{'kind':'elapsed','ms':True}], [{'kind':'predicate'}],
                    [{'kind':'elapsed','ms':0},{'kind':'elapsed','ms':1}],
                    [{'kind':'text','encoding':'raw','needle':[256]}],
                    [{'kind':'elapsed','ms':0,'unknown':1}]):
  bad=req('execution.continue',{'session_id':loop_id,'stop_any':conditions})
  assert 'error' in bad,bad
 assert req('state.get_registers',{'session_id':loop_id})['result']==before
 def conditioned(conditions):
  admitted=req('execution.continue',{'session_id':loop_id,'stop_any':conditions})
  assert admitted['result']['state']=='running',admitted
  done=req('execution.wait',{'session_id':loop_id,'operation_id':admitted['result']['operation_id'],'timeout_ms':1000})
  if done['result']['state']!='stopped': req('session.status',{'session_id':loop_id})
  assert done['result']['state']=='stopped',done
  assert done['result']['stop_reason']['kind']=='condition',done
  return done['result']['stop_reason']
 zero=conditioned([{'kind':'elapsed','ms':0}])
 assert zero['condition_index']==1 and not zero['guest_advanced'],zero
 regs=req('state.get_registers',{'session_id':loop_id})['result']
 assert regs['instruction_pointer']==before['instruction_pointer'],regs
 address=conditioned([{'kind':'address','cs':int(regs['segments']['cs'],16),'ip':256}])
 assert not address['guest_advanced'],address
 for clock in ('guest_ms','host_monotonic'):
  stop=conditioned([{'kind':'elapsed','clock':clock,'ms':25}])
  assert stop['guest_advanced'],stop
  assert stop['end_tick']>=stop['start_tick'],stop
 text=conditioned([{'kind':'text','encoding':'raw','needle':[32]}])
 assert text['text']['cells'] and not text['guest_advanced'],text
 print('REAL_RPC_STOP_CONDITIONS_PASS',flush=True)
 running=req('execution.continue',{'session_id':loop_id},'loop-run')
 timed=req('execution.wait',{'session_id':loop_id,'operation_id':running['result']['operation_id'],'timeout_ms':1},'loop-wait')
 assert timed.get('result',{}).get('running') is True,timed
 # Drop the transport while native continuous RUN is active. Reconnection
 # must retain the operation/fence, not reinterpret loss as exit or replay.
 c.close()
 c=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);c.connect(str(w/'rpc.sock'));c.setblocking(False)
 active_after_loss=req('session.status',{'session_id':loop_id})
 assert active_after_loss['result']['state']=='running',active_after_loss
 denied=req('state.get_registers',{'session_id':loop_id})
 assert denied.get('error',{}).get('data',{}).get('reason')=='TARGET_RUNNING',denied
 denied=req('memory.read',{'session_id':loop_id,'address':{'space':'physical','offset':'0x00000100'},'length':1})
 assert 'error' in denied,denied
 replay=req('execution.continue',{'session_id':loop_id},'loop-run')
 assert replay==running,(replay,running)
 paused=req('execution.pause',{'session_id':loop_id})
 pause_wait=req('execution.wait',{'session_id':loop_id,'operation_id':paused['result']['operation_id'],'timeout_ms':1000})
 assert pause_wait['result']['state']=='stopped',pause_wait
 assert pause_wait['result']['stop_reason']['kind']=='pause',pause_wait
 req('session.stop',{'session_id':loop_id})
 drain(1)
 loop_done=req('session.status',{'session_id':loop_id})
 assert loop_done['result']['state']=='exited',loop_done
 resident=req('session.start',{'target':{'command':'STAY.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'resident')
 assert 'result' in resident,resident
 resident_id=resident['result']['session_id']
 req('execution.continue',{'session_id':resident_id},'resident-run')
 drain(2)
 resident_status=req('session.status',{'session_id':resident_id})
 assert resident_status['result']['state']!='exited',resident_status
 denied=req('session.start',{'target':{'command':'TINY.COM','arguments':[]},'mounts':[{'drive':'C','host_path':str(w)}],'break_at':'entry'},'resident-refusal')
 assert 'error' in denied,denied
finally:
 if c:c.close()
 p.terminate()
 try:rc=p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.kill();rc=p.wait()
 drain(.1);os.close(m);log.close();x.terminate();xr=x.wait(timeout=5);xl.close();print('cleanup',rc,xr,flush=True)

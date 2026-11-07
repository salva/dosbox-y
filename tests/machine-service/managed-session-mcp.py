"""Actual installed stdio MCP default/custom DOSBox consumer smoke; no agent."""
import os,pathlib,subprocess,json,select,time,tempfile
w=pathlib.Path(tempfile.mkdtemp(prefix='managed-mcp-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
(w/'TINY.COM').write_bytes(bytes.fromhex('90 eb fd'))
(w/'fixture.conf').write_text(f'[sdl]\noutput=surface\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c {w}\nc:\nTINY.COM\n')
r,wr=os.pipe();log=open(w/'xvfb.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1024x768x24'],pass_fds=(wr,),stdout=log,stderr=log);os.close(wr)
with os.fdopen(r) as pipe:display=pipe.readline().strip()
prefix=pathlib.Path(os.environ['DOSBOX_SESSION_INSTALL'])
env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy',LUA_PATH=str(prefix/'share/lua/5.4/?.lua')+';'+str(prefix/'share/lua/5.4/?/init.lua')+';;',LUA_CPATH=str(prefix/'lib/lua/5.4/?.so')+';;')
for k in ('LUA_PATH_5_4','LUA_CPATH_5_4'):env.pop(k,None)
stderr=open(w/'mcp.log','wb');p=subprocess.Popen([str(prefix/'bin/lua-repl-mcp'),'--startup-timeout-ms','20000'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=stderr,env=env,cwd=w,bufsize=0)
buf=b'';seq=0
sessions=[]
def rpc(method,params):
 global seq,buf
 seq+=1;p.stdin.write((json.dumps(dict(jsonrpc='2.0',id=seq,method=method,params=params))+'\n').encode())
 end=time.monotonic()+30
 while b'\n' not in buf:
  assert select.select([p.stdout],[],[],max(0,end-time.monotonic()))[0],'timeout'
  data=os.read(p.stdout.fileno(),65536);assert data,'EOF';buf+=data
 line,buf=buf.split(b'\n',1);v=json.loads(line);assert v['id']==seq and 'error' not in v,v
 return v['result']
def call(name,**args):
 v=rpc('tools/call',dict(name=name,arguments=args))['structuredContent'];print(name,v,flush=True);return v
try:
 rpc('initialize',dict(protocolVersion='2025-11-25',capabilities={},clientInfo=dict(name='dosbox-host',version='1')))
 p.stdin.write(b'{"jsonrpc":"2.0","method":"notifications/initialized"}\n')
 assert len(rpc('tools/list',{})['tools'])==5
 for command in (None,[os.environ['DOSBOX_TEST_BINARY'],'-conf',str(w/'fixture.conf'),'-lua-driver-session','-lua-driver-program','C:/TINY.COM']):
  created=call('session_create',**({'command':command} if command else {}));sid=created['session_id'];sessions.append(sid)
  code="host_count=(host_count or 0)+1; assert(io and os and debug); return host_count,require('lua_repl_mcp.bootstrap')~=nil"
  assert call('session_execute',session_id=sid,code=code,wait_timeout_ms=None)['values']==[1,True]
  if command:
   assert call('session_execute',session_id=sid,code="local d=require('dosbox');return d.registers(d.context()).registers_valid",wait_timeout_ms=None)['values']==[True]
   assert call('session_execute',session_id=sid,code="return os.execute('test ! -e /proc/self/fd/3')",wait_timeout_ms=None)['values'][0] is True
  assert call('session_execute',session_id=sid,code='return host_count')['values']==[1]
  pending=call('session_execute',session_id=sid,code='while true do end',wait_timeout_ms=0);assert pending['status']=='running'
  assert call('session_wait',session_id=sid,wait_timeout_ms=1)['status']=='running'
  assert call('session_kill',session_id=sid)['process_exit_confirmed'];sessions.remove(sid)
 print('PASS installed MCP default and DOSBox custom create/execute/wait/kill',w,flush=True)
finally:
 for sid in sessions:
  try:call('session_kill',session_id=sid)
  except Exception:pass
 p.stdin.close()
 try:p.wait(timeout=15)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 x.terminate();x.wait(timeout=10);log.close();stderr.close()

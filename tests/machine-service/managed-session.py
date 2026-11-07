"""Combined pinned-module / real native worker regression. No live service."""
import os, pathlib, socket, subprocess, tempfile, json, time, select, zipfile, hashlib
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='managed-',dir=root))
module=pathlib.Path(os.environ['DOSBOX_SESSION_MODULE'])
binary=os.environ['DOSBOX_TEST_BINARY']
limits=dict(input_bytes=1048576,values_bytes=1048576,diagnostic_bytes=65536,max_depth=64,worker_bytes=4194304,response_bytes=16777216,output_bytes=33554432)
(w/'TINY.COM').write_bytes(bytes.fromhex('b8 2a 4c cd 21'))
(w/'fixture.conf').write_text(f'[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c {w}\nc:\nTINY.COM\n')
(w/'probe.c').write_text('#include <lua.h>\nint luaopen_probe(lua_State *L){lua_pushinteger(L,42);return 1;}\n')
subprocess.run(['cc','-shared','-fPIC',str(w/'probe.c'),'-o',str(w/'probe.so')]+subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True).split(),check=True)
r,wr=os.pipe();xl=open(w/'xvfb.log','wb')
x=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1024x768x24'],pass_fds=(wr,),stdout=xl,stderr=xl);os.close(wr)
with os.fdopen(r) as pipe: display=pipe.readline().strip()
children=[]
def launch(restore=None):
 a,b=socket.socketpair()
 # Map FD3 in an exec helper, avoiding Python's close_fds ordering/aliases.
 env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy',LUA_PATH=str(module/'?.lua')+';'+str(module/'?/init.lua')+';;',LUA_CPATH=str(w/'?.so')+';;',LUA_REPL_MCP_SESSION_FD='3',LUA_REPL_MCP_ALLOCATION_GENERATION='1',LUA_REPL_MCP_LIMITS=json.dumps(limits))
 args=[binary,'-conf',str(w/'fixture.conf'),'-lua-driver-session']
 args+=['-restore-state',str(restore)] if restore else ['-lua-driver-program','C:/TINY.COM']
 log=open(w/('host-'+str(len(children))+'.log'),'wb')
 p=subprocess.Popen(['python3','-c','import os,sys; os.dup2(int(sys.argv[1]),3); os.set_inheritable(3,True); os.execv(sys.argv[2],sys.argv[2:])',str(b.fileno())]+args,pass_fds=(b.fileno(),),stdout=log,stderr=log,cwd=w,env=env)
 b.close();a.settimeout(20);f=a.makefile('rb');children.append((p,a,f,log))
 ready=json.loads(f.readline());assert ready['type']=='ready',ready
 print('ready',p.pid,ready,flush=True)
 return p,a,f
seq={}
def execute(a,f,code,trailing=False):
 n=seq.get(a,0)+1;seq[a]=n
 record=json.dumps(dict(type='execute',protocol_version=1,allocation_generation=1,execution_generation=n,input={'code':code}))+'\n'
 if trailing:
  record+=json.dumps(dict(type='execute',protocol_version=1,allocation_generation=1,execution_generation=n+1,input={'code':"local f=assert(io.open('forbidden','w'));f:write('bad');f:close()"}))+'\n'
 a.sendall(record.encode())
 v=json.loads(f.readline());assert v['type']=='result',v
 print('result',n,v,flush=True)
 result=v['result']
 if "error('managed-marker')" in code:
  assert result['status']=='failed' and result['error']['code']=='lua_runtime_error',result
 else: assert result['status']=='completed',result
 return result
try:
 p,a,f=launch()
 execute(a,f,"assert(require('probe')==42 and io and os and debug and coroutine); persistent=17; local d=require('dosbox'); c=d.context(); return {answer=42}")
 execute(a,f,"assert(persistent==17); persistent=persistent+1; return persistent")
 execute(a,f,"error('managed-marker')")
 execute(a,f,"local out=assert(io.open('once','a'));out:write('x');out:close();return true")
 assert((w/'once').read_text()=='x')
 execute(a,f,"return require('dosbox').save('"+str(w/'source.sav')+"').effects.completed")
 a.sendall(b'{"type":"close","protocol_version":1,"allocation_generation":1}\n')
 # Private close settles worker but ordinary host is still stopped/alive.
 assert f.readline()==b''
 p.terminate();p.wait(timeout=10)
 source_hash=hashlib.sha256((w/'source.sav').read_bytes()).hexdigest()
 p,a,f=launch(w/'source.sav')
 before=execute(a,f,"local d=require('dosbox'); c=d.context(); before=d.registers(c); return {ip=before.registers.ip,cs=before.registers.cs}")['values'][0]
 execute(a,f,"return require('dosbox').save('"+str(w/'held1.sav')+"').effects.completed")
 time.sleep(.1)
 execute(a,f,"return require('dosbox').save('"+str(w/'held2.sav')+"').effects.completed")
 with zipfile.ZipFile(w/'held1.sav') as one,zipfile.ZipFile(w/'held2.sav') as two:
  for name in ('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10'):
   assert one.read(name)==two.read(name),(name,'advanced while session idle')
 assert hashlib.sha256((w/'source.sav').read_bytes()).hexdigest()==source_hash
 after=execute(a,f,"local d=require('dosbox');local r=d.registers(d.context());return {ip=r.registers.ip,cs=r.registers.cs}")['values'][0]
 assert before==after,(before,after)
 result=execute(a,f,"local r=require('dosbox').run(); return {reason=r.reason,exit_code=r.exit_code}",trailing=True)
 assert result['values'][0]['reason']=='root_exit',result
 try: assert f.readline()==b'', 'terminal must bar later chunks'
 except ConnectionResetError: pass # unread trailing input may reset the socket
 assert not (w/'forbidden').exists()
 p.wait(timeout=10)
 print('PASS combined managed session',w,flush=True)
finally:
 for p,a,f,log in children:
  if p.poll() is None:p.terminate()
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  f.close();a.close();log.close()
 x.terminate();x.wait(timeout=10);xl.close()

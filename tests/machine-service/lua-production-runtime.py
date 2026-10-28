import os,pathlib,tempfile,subprocess,pty,fcntl,termios,struct,time,select,json
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'lua-production-runtime'
root.mkdir(exist_ok=True)
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root))
(w/'TINY.COM').write_bytes(bytes.fromhex('90 40 eb fc'))
(w/'fixture.conf').write_text(f'[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c {w}\nc:\nTINY.COM\n')
(w/'module.c').write_text('#include <lua.h>\n#include <lauxlib.h>\nint luaopen_probe(lua_State *L){lua_pushinteger(L,42);return 1;}\n')
subprocess.run(['cc','-shared','-fPIC',str(w/'module.c'),'-o',str(w/'probe.so')]+subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True).split(),check=True)
(w/'fromfile.lua').write_text('return 17')
script='''local d=require('dosbox')
assert(require('probe')==42 and dofile('fromfile.lua')==17)
assert(os and io and debug and coroutine and package)
local final=setmetatable({},{__gc=function() local out=assert(io.open('FINAL_PATH','w'));out:write('closed');out:close();assert(not pcall(d.step)) end})
local f=assert(io.open('RESULT_PATH','w'));f:write('started\\n');f:flush()
local c=assert(d.context());local r=d.registers(c);assert(r.registers_valid)
f:write('registers '..r.registers.cs..':'..r.registers.ip..'\\n');f:flush()
local s=d.step();assert(s.context);local sr=d.registers(s.context);assert(sr.registers.cs~=61440 and sr.registers.ip==256);f:write('step '..s.reason..' '..sr.registers.cs..':'..sr.registers.ip..'\\n');f:flush()
local calls=0;local bp=d.breakpoint(sr.registers.cs,257,function(image) calls=calls+1;assert(d.read(image.context,sr.registers.cs*16+257,1));return true end)
local hit=d.run();f:write('hit '..tostring(hit.reason)..' calls '..calls..' error '..tostring(hit.error)..'\\n');f:flush();assert(hit.reason=='breakpoint' and calls==1);assert(d.remove(bp.registration))
f:write('callback PASS\\n');f:flush()
local run=d.run_until_timeout(3);assert(run.context);f:write('run '..run.reason..'\\n');f:flush()
local saved=d.save('SAVE_PATH',30000)
f:write('save '..tostring(saved.effects and saved.effects.completed)..' '..tostring(saved.error)..'\\n');f:flush()
assert(saved.effects and saved.effects.completed and saved.effects.retired)
local saved2=d.save('SAVE2_PATH',30000);assert(saved2.effects and saved2.effects.completed and saved2.effects.retired)
f:write('PASS\\n');f:close()
'''.replace('RESULT_PATH',str(w/'result.txt')).replace('SAVE_PATH',str(w/'one.sav')).replace('SAVE2_PATH',str(w/'two.sav')).replace('FINAL_PATH',str(w/'final.txt'))
if os.environ.get('LUA_TEST_STATE'):
 script='''local d=require('dosbox');local f=assert(io.open('RESULT_PATH','w'))
local c=assert(d.context());local r=d.registers(c);assert(r.registers_valid)
assert(r.registers.cs==2068 and r.registers.ip>=256 and r.registers.ip<=258)
local before=d.read(c,2068*16+256,4);assert(before)
local s=d.step();assert(s.context);assert(d.save('SAVE_PATH',30000).effects.completed)
f:write('\\nPASS\\n');f:close()
'''.replace('RESULT_PATH',str(w/'result.txt')).replace('SAVE_PATH',str(w/'restored.sav'))
if os.environ.get('LUA_TEST_TIMEOUT'):
 script=script.replace("local saved=d.save('"+str(w/'one.sav')+"',30000)", "local saved=d.save('"+str(w/'one.sav')+"',1)")
 script=script.replace('assert(saved.effects and saved.effects.completed and saved.effects.retired)', "assert(saved.effects and not saved.effects.completed and saved.effects.retired and saved.error=='timeout');assert(not io.open('"+str(w/'one.sav')+"','rb'))")
if os.environ.get('LUA_TEST_ERROR'):
 script += "\nerror('production-error-marker')\n"
(w/'worker.lua').write_text(script)
r,wr=os.pipe();xl=open(w/'xvfb.log','wb');x=subprocess.Popen(['Xvfb','-displayfd',str(wr),'-screen','0','1024x768x24'],pass_fds=(wr,),stdout=xl,stderr=xl);os.close(wr)
with os.fdopen(r) as pipe:display=pipe.readline().strip()
master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0));os.set_blocking(master,False)
log=open(w/'pty.log','wb')
binary=os.environ['DOSBOX_TEST_BINARY']
argv=[binary,'-conf',str(w/'fixture.conf'),'-lua-driver-code',script] if os.environ.get('LUA_TEST_INLINE') else [binary,'-conf',str(w/'fixture.conf'),'-lua-driver-file',str(w/'worker.lua')]
argv+=['-lua-driver-program','C:/TINY.COM']
if os.environ.get('LUA_TEST_STATE'):argv+=['-lua-driver-state',os.environ['LUA_TEST_STATE']]
p=subprocess.Popen(argv,stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,DISPLAY=':'+display,TERM='xterm',SDL_AUDIODRIVER='dummy'),cwd=w);os.close(slave)
print(w,flush=True)
try:
 end=time.monotonic()+35
 while time.monotonic()<end:
  if select.select([master],[],[],.05)[0]:
   try:log.write(os.read(master,65536));log.flush()
   except OSError:break
  if p.poll() is not None:break
  if (w/'final.txt').exists() and p.poll() is not None:break
 result=(w/'result.txt').read_text() if (w/'result.txt').exists() else 'NO RESULT'
 print(result,flush=True)
 assert '\nPASS\n' in result,result
 if not os.environ.get('LUA_TEST_STATE'):assert (w/'final.txt').read_text()=='closed'
 rc=p.wait(timeout=5)
 if os.environ.get('LUA_TEST_ERROR'):
  assert rc!=0,rc
  assert b'production-error-marker' in (w/'pty.log').read_bytes()
 else:assert rc==0,rc
finally:
 if p.poll() is None:p.terminate()
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 x.terminate();x.wait(timeout=5);os.close(master);log.close();xl.close()

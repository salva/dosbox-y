"""Real FILE/INLINE root terminal response, Lua close, and process disposal."""
import hashlib, os, pathlib, subprocess, tempfile
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='terminal-',dir=root/'fresh-native-entry'))
archive=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
before=hashlib.sha256(archive.read_bytes()).hexdigest()
exit_code=int(os.environ.get('DOSBOX_TEST_EXIT_CODE','42'))
assert 0<=exit_code<=255
conf=w/'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
if os.environ.get('DOSBOX_TEST_BLOCKED_FINALIZER'):
 # An actual Lua C call cannot be preempted by hooks. Keep the worker/process
 # owned until this invocation's release file lets lua_close finish.
 subprocess.run(['cc','-shared','-fPIC','-x','c','-','-o',str(w/'close_block.so')]+
  subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True).split(),
  input='''#include <lua.h>
#include <lauxlib.h>
#include <stdio.h>
#include <unistd.h>
static int block(lua_State *L) {
 const char *entered=luaL_checkstring(L,1), *release=luaL_checkstring(L,2);
 FILE *f=fopen(entered,"w");if(!f)return luaL_error(L,"marker open failed");
 fputs("entered",f);fclose(f);
 while(access(release,F_OK)!=0)usleep(10000);
 return 0;
}
int luaopen_close_block(lua_State *L){lua_pushcfunction(L,block);return 1;}
''',text=True,check=True)
for mode in ('file','code'):
 done=w/(mode+'-closed'); receipt=w/(mode+'-terminal'); gc_done=w/(mode+'-gc-closed')
 entered=w/(mode+'-c-entered');release=w/(mode+'-c-release')
 script=f'''local d=require('dosbox')
-- Retain a reachable object until lua_close, rather than confusing lexical
-- <close> at script return with the worker's luaClosed retirement fence.
_G.close_probe=setmetatable({{}},{{__gc=function()
 local ok=pcall(d.context)
 local f=assert(io.open('{gc_done}','w'));f:write(ok and 'UNSAFE' or 'revoked');f:close()
end}})
local x <close> = setmetatable({{}},{{__close=function()
 local ok=pcall(d.context)
 local f=assert(io.open('{done}','w'));f:write(ok and 'UNSAFE' or 'revoked');f:close()
end}})
local r=d.run{{}}
assert(r.reason=='root_exit' and r.boundary=='restored-root-exit')
assert(r.exit_code=={exit_code} and r.root_psp>0)
assert(r.resident=={str(bool(os.environ.get('DOSBOX_TEST_RESIDENT'))).lower()} and r.self_parent=={str(bool(os.environ.get('DOSBOX_TEST_SELF_PARENT'))).lower()})
assert(not r.continuation and r.context==nil)
local ok=pcall(d.context);assert(not ok)
local f=assert(io.open('{receipt}','w'));f:write(r.exit_code);f:close()
'''
 if os.environ.get('DOSBOX_TEST_OBSERVE_BEFORE_RUN'):
  held_one=w/(mode+'-held-one.sav');held_two=w/(mode+'-held-two.sav')
  observation=f'''local c=d.context()
local regs=d.registers(c)
assert(d.save('{held_one}').effects.usable)
os.execute('sleep 2')
assert(d.save('{held_two}').effects.usable)
'''
  script=script.replace('local r=d.run{}',observation+'local r=d.run{}')
 if os.environ.get('DOSBOX_TEST_BLOCKED_FINALIZER'):
  script=script.replace("local d=require('dosbox')",f"local d=require('dosbox');package.cpath='{w}/?.so;'..package.cpath;local block=require('close_block')")
  script=script.replace(" local ok=pcall(d.context)",f" block('{entered}','{release}');local ok=pcall(d.context)",1)
 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR'):
  # Inject after each refusal receipt, so both lexical __close and actual
  # lua_close __gc error, without losing the evidence that access was revoked.
  script=script.replace("f:close()\nend", "f:close();error('expected close-fence error')\nend")
 path=w/(mode+'.lua');path.write_text(script)
 with (w/(mode+'.log')).open('wb') as log:
  argv=[os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-'+mode,str(path) if mode=='file' else script]
  with subprocess.Popen(argv,stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm')) as p:
   try:
    if os.environ.get('DOSBOX_TEST_BLOCKED_FINALIZER'):
     import time
     deadline=time.monotonic()+30
     while not entered.exists() and p.poll() is None and time.monotonic()<deadline:time.sleep(.02)
     assert entered.exists() and p.poll() is None,(w,mode,'no blocked C finalizer')
     assert receipt.read_text()==str(exit_code) and done.read_text()=='revoked'
     assert not gc_done.exists()
     time.sleep(.5)
     assert p.poll() is None and not gc_done.exists(),(w,mode,'retired beneath C finalizer')
     release.write_text('release invocation-owned block')
    rc=p.wait(timeout=40)
   finally:
    release.touch(exist_ok=True)
    if p.poll() is None:
     p.terminate()
     try:p.wait(timeout=5)
     except subprocess.TimeoutExpired:p.kill();p.wait()
 assert rc==(1 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR') else 0),(w,mode,rc)
 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR'):
  text=(w/(mode+'.log')).read_text(errors='replace')
  assert 'expected close-fence error' in text and 'Restored session ended:' in text
 if os.environ.get('DOSBOX_TEST_OBSERVE_BEFORE_RUN'):
  import zipfile
  with zipfile.ZipFile(archive) as original,zipfile.ZipFile(held_one) as one,zipfile.ZipFile(held_two) as two:
   for name in ('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10'):
    assert one.read(name)==two.read(name),(w,mode,name,'advanced before explicit Run')
   for name in ('Pic','Dosbox-x','Keyboard','IntTimer10'):
    assert original.read(name)==one.read(name),(w,mode,name,'advanced before Lua observation')
 assert done.read_text()=='revoked' and receipt.read_text()==str(exit_code),(w,mode)
 assert gc_done.read_text()=='revoked',(w,mode,'lua_close finalizer granted a context')
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print(w,'FILE/INLINE exact root exit response, revoked context, finalizer, process disposal passed')

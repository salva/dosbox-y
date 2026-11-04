"""Worker FILE/INLINE restore probe with compatible engine/ABI and guest hardware."""
import os,pathlib,tempfile,subprocess,zipfile,hashlib,time
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='lua-',dir=root/'fresh-native-entry'))
archive=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
before=hashlib.sha256(archive.read_bytes()).hexdigest()
conf=w/'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
subprocess.run(['cc','-shared','-fPIC','-x','c','-','-o',str(w/'restored_probe.so')]+
 subprocess.check_output(['pkg-config','--cflags','--libs','lua5.4'],text=True).split(),
 input='#include <lua.h>\nint luaopen_restored_probe(lua_State *L){lua_pushinteger(L,42);return 1;}\n',text=True,check=True)
(w/'fromfile.lua').write_text('return 17')
for mode in ('file','code'):
 a=w/(mode+'-one.sav');b=w/(mode+'-two.sav');done=w/(mode+'-closed')
 script=f'''local d=require('dosbox')
assert(io and os and package and debug and coroutine)
package.cpath='{w}/?.so;'..package.cpath
assert(require('restored_probe')==42 and dofile('{w}/fromfile.lua')==17)
assert(not pcall(function() error('restored-error-probe') end))
local c=d.context()
local r=d.registers(c)
assert(d.save('{a}').effects.usable)
os.execute('sleep 2')
assert(d.save('{b}').effects.usable)
local x <close> = setmetatable({{}},{{__close=function() local f=assert(io.open('{done}','w')); f:write('closed'); f:close() end}})
'''
 path=w/(mode+'.lua');path.write_text(script)
 arg=str(path) if mode=='file' else script
 log=(w/(mode+'.log')).open('wb')
 p=subprocess.Popen([os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-'+mode,arg],stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'))
 try:
  deadline=time.monotonic()+35
  while not done.exists() and time.monotonic()<deadline and p.poll() is None:time.sleep(.05)
  assert done.exists(),(w,mode,p.poll())
  rc=p.wait(timeout=10)
  assert rc==0,(w,mode,rc)
  with zipfile.ZipFile(archive) as source,zipfile.ZipFile(a) as one,zipfile.ZipFile(b) as two:
   import re
   cpu_prefix,descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(archive.parent/'layout.txt').read_text()))
   assert source.read('CPU')[:cpu_prefix]==one.read('CPU')[:cpu_prefix],(mode,'guest CPU changed before worker')
   assert source.read('Memory')[descriptor:]==one.read('Memory')[descriptor:],(mode,'guest RAM changed before worker')
   for name in ('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10'):
    assert one.read(name)==two.read(name),(mode,name,'advanced during worker delay')
   for name in ('Pic','Dosbox-x','Keyboard','IntTimer10'):
    assert source.read(name)==one.read(name),(mode,name,'advanced before worker')
  assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
 finally:
  p.terminate()
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:p.kill();p.wait()
  log.close()
print(w,'FILE/INLINE worker held save/delay/save and Lua close finalizer passed')

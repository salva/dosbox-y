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
for mode in ('file','code'):
 done=w/(mode+'-closed'); receipt=w/(mode+'-terminal'); gc_done=w/(mode+'-gc-closed')
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
 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR'):
  # Inject after each refusal receipt, so both lexical __close and actual
  # lua_close __gc error, without losing the evidence that access was revoked.
  script=script.replace("f:close()\nend", "f:close();error('expected close-fence error')\nend")
 path=w/(mode+'.lua');path.write_text(script)
 with (w/(mode+'.log')).open('wb') as log:
  p=subprocess.run([os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-'+mode,str(path) if mode=='file' else script],stdout=log,stderr=log,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'),timeout=40)
 assert p.returncode==(1 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR') else 0),(w,mode,p.returncode)
 if os.environ.get('DOSBOX_TEST_FINALIZER_ERROR'):
  text=(w/(mode+'.log')).read_text(errors='replace')
  assert 'expected close-fence error' in text and 'Restored session ended:' in text
 assert done.read_text()=='revoked' and receipt.read_text()==str(exit_code),(w,mode)
 assert gc_done.read_text()=='revoked',(w,mode,'lua_close finalizer granted a context')
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print(w,'FILE/INLINE exact root exit response, revoked context, finalizer, process disposal passed')

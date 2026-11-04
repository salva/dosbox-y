"""Diagnostic actual startup import. No success claim from archive open alone."""
import os, pathlib, subprocess, tempfile
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=root/'fresh-native-entry'))
conf=w/'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
cmd=[os.environ['DOSBOX_TEST_BINARY'],'-logfile',str(w/'native.log'),'-conf',str(conf),'-restore-state',os.environ['DOSBOX_TEST_ARCHIVE']]
with (w/'output.log').open('wb') as out:
 p=subprocess.Popen(cmd,stdout=out,stderr=out,env=dict(os.environ,SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy',TERM='xterm'))
 try: result=p.wait(timeout=20)
 except subprocess.TimeoutExpired:
  p.terminate()
  try: p.wait(timeout=10)
  except subprocess.TimeoutExpired: p.kill();p.wait()
  result='timeout-terminated'
print(w, 'process-result',result)
print((w/'output.log').read_text(errors='replace')[-6000:])

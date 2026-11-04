"""Real minizip top-level native fclose error after full install: discard target.
Linux author witness. Inject only source fclose, not owner/serializer results.
"""
import os,pathlib,tempfile,subprocess,hashlib
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='close-',dir=root/'fresh-native-entry'))
archive=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE']).resolve()
before=hashlib.sha256(archive.read_bytes()).hexdigest()
shim=w/'source-close-failure.so'
subprocess.run(['cc','-shared','-fPIC',str(pathlib.Path(__file__).with_name('source-close-failure.c')),'-ldl','-o',str(shim)],check=True)
conf=w/'fixture.conf'
conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n')
marker=w/'evaluator-entered'
args=[os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive),'-lua-driver-code',f"assert(io.open('{marker}','w')):close()"]
env=dict(os.environ,LD_PRELOAD=str(shim),DOSBOX_TEST_CLOSE_SOURCE=str(archive),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm')
result=subprocess.run(args,capture_output=True,env=env,timeout=40)
(w/'native.log').write_bytes(result.stdout+result.stderr)
assert result.returncode==1,(w,result.returncode)
assert not marker.exists()
# GDB must actually run with interposition (not its default shell intermediate).
gdb=['gdb','-batch','-ex','set startup-with-shell off','-ex','set environment LD_PRELOAD '+str(shim),'-ex','set environment DOSBOX_TEST_CLOSE_SOURCE '+str(archive),'-ex','break E_Exit','-ex','run','-ex','x/s $rsi','-ex','bt','-ex','up','-ex','p installed','-ex','p closed','--args']+args
proof=subprocess.run(gdb,capture_output=True,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'),timeout=40)
(w/'diagnosis.log').write_bytes(proof.stdout+proof.stderr)
assert proof.returncode==0
assert b'Fresh restore failed; disposable target cannot continue' in proof.stdout,(w,proof.stdout)
assert b'SaveState::restoreFreshStartup' in proof.stdout
assert b'SaveState::install' not in proof.stdout
assert b'$1 = true' in proof.stdout,(w,'did not finish install before close failure')
assert not marker.exists()
assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
print(w,'full-install top-level archive fclose error: exit1/no evaluator/source unchanged')

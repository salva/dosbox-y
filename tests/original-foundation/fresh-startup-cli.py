"""Real pre-initialization CLI refusal, not a startup restore success witness."""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='cli-',dir=root/'fresh-native-entry'))
binary=os.environ['DOSBOX_TEST_BINARY']
for index,args in enumerate((['-restore-state'],['-restore-state',''],
                             ['-restore-state','a','-restore-state','b'])):
 diagnosis=subprocess.run(['gdb','-batch','-ex','break VM_ConstructFreshNative()',
                          '-ex','run','-ex','bt','--args',binary,*args],cwd=w,
                         capture_output=True,text=True,timeout=30,
                         env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'))
 (w/f'{index}.gdb.log').write_text(diagnosis.stdout+diagnosis.stderr)
 assert 'exited with code' in diagnosis.stdout,diagnosis.stdout+diagnosis.stderr
 assert 'hit Breakpoint' not in diagnosis.stdout,diagnosis.stdout
 result=subprocess.run([binary,*args],cwd=w,capture_output=True,timeout=15,
                       env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'))
 (w/f'{index}.log').write_bytes(result.stdout+result.stderr)
 assert result.returncode!=0,(args,result.returncode,result.stdout,result.stderr)
print(w,'missing/empty/duplicate restore CLI refuses (nonzero process exit)')

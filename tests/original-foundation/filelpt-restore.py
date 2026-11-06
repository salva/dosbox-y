#!/usr/bin/env python3
"""Actual LPT open append stream/ACK/addLF/current-content fresh continuation."""
import os,pathlib,subprocess,tempfile
here=pathlib.Path(__file__).resolve().parent
w=pathlib.Path(tempfile.mkdtemp(prefix='filelpt-restore-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(w,flush=True)
env=dict(os.environ,DOSBOX_TEST_FILELPT='1',DOSBOX_TEST_CAPTURE_IP='0814:0200',DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0201')
if env.get('DOSBOX_TEST_FILELPT_ACTION'):
 env['DOSBOX_TEST_FILELPT_FLAGS']=env.get('DOSBOX_TEST_FILELPT_FLAGS','addLF')+' openwith:"touch '+str(w/'action-ran')+'"'
p=subprocess.run(['python3',str(here/'stopped-save.py')],env=env,capture_output=True,text=True)
(w/'producer.log').write_text(p.stdout+p.stderr);p.check_returncode()
base=pathlib.Path(p.stdout.splitlines()[0]);target=base/'printed.prt'
assert target.read_bytes()==b'current host prefix:'+b'A'*int(os.environ.get('DOSBOX_TEST_FILELPT_PREFIX','0'))+b'\r',target.read_bytes()
# Current content appended after capture must not disappear/replay on import.
with target.open('ab') as f:f.write(b'EXTERNAL')
before=target.read_bytes()
env.update(DOSBOX_TEST_ARCHIVE=str(base/'first.sav'),DOSBOX_TEST_STEP_IP='0814:0201',DOSBOX_TEST_ROOT_EXIT='1',DOSBOX_TEST_ROOT_FACTS='code=42 resident=0 self-parent=0')
refusal=os.environ.get('DOSBOX_TEST_FILELPT_REFUSAL')
if refusal:
 assert refusal in ('missing','fifo','directory','binding')
 # Preserve unique producer output; replace only this task-owned target.
 if refusal!='binding':target.rename(base/'printed-preserved.prt')
 else:env['DOSBOX_TEST_FILELPT_DEST_FLAGS']=env.get('DOSBOX_TEST_FILELPT_FLAGS','addLF')+' shellhide'
 if refusal=='fifo':os.mkfifo(target)
 elif refusal=='directory':target.mkdir()
 env['DOSBOX_TEST_RESTORE_REFUSAL']='1'
p=subprocess.run(['python3',str(here/'fresh-startup.py')],env=env,capture_output=True,text=True)
(w/'restore.log').write_text(p.stdout+p.stderr);p.check_returncode()
if refusal:
 assert (target if refusal=='binding' else base/'printed-preserved.prt').read_bytes()==before
 if refusal=='binding':pass
 elif refusal=='missing':assert not target.exists()
 elif refusal=='fifo':
  import stat
  assert stat.S_ISFIFO(target.stat().st_mode)
 else:assert target.is_dir() and not list(target.iterdir())
 print('FileLPT '+refusal+' startup refusal/closed transport/no host mutation PASS')
 raise SystemExit(0)
assert target.read_bytes()==before+b'X\n'+(b'\x0c' if os.environ.get('DOSBOX_TEST_FILELPT_WAIT') and 'addFF' in env.get('DOSBOX_TEST_FILELPT_FLAGS','') else b''),target.read_bytes()
if os.environ.get('DOSBOX_TEST_FILELPT_ACTION'):
 assert (w/'action-ran').exists(),list(w.iterdir())
print('FileLPT current host append/ACK continuation/addLF/no replay/root42 PASS')

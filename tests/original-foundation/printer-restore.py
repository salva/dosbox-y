#!/usr/bin/env python3
"""Real stopped capture/fresh debugger continuation of Epson idle/pending jobs."""
import os,pathlib,subprocess,tempfile,zipfile
here=pathlib.Path(__file__).resolve().parent
w=pathlib.Path(tempfile.mkdtemp(prefix='printer-restore-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(w,flush=True)
for mode in range(4):
 env=dict(os.environ,DOSBOX_TEST_PRINTER=str(mode),DOSBOX_TEST_CAPTURE_IP='0814:0200',DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0201')
 p=subprocess.run(['python3',str(here/'stopped-save.py')],env=env,capture_output=True,text=True)
 (w/f'{mode}-producer.log').write_text(p.stdout+p.stderr);p.check_returncode()
 base=pathlib.Path(p.stdout.splitlines()[0]);archive=base/'first.sav'
 with zipfile.ZipFile(archive) as z:
  assert z.read('Printer')[0]==int(mode in (1,2)),(mode,z.read('Printer'))
 before={p.name:p.read_bytes() for p in base.glob('*.ps')}
 env.update(DOSBOX_TEST_ARCHIVE=str(archive),DOSBOX_TEST_STEP_IP='0814:0201',DOSBOX_TEST_ROOT_EXIT='1',DOSBOX_TEST_ROOT_FACTS='code=42 resident=0 self-parent=0')
 p=subprocess.run(['python3',str(here/'fresh-startup.py')],env=env,capture_output=True,text=True)
 (w/f'{mode}-restore.log').write_text(p.stdout+p.stderr);p.check_returncode()
 after={p.name:p.read_bytes() for p in base.glob('*.ps')}
 if mode in (1,2):assert after==before,'faulted restore emitted pending output'
 else:assert len(after)>len(before),'idle restore failed to print new page'
 print(mode,'idle/clean usable' if mode in (0,3) else 'pending direct/BIOS/DOS fault, no replay','PASS',flush=True)

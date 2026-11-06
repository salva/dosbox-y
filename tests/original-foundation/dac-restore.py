#!/usr/bin/env python3
"""Actual guest DAC pending-triplet fresh process continuation/readback."""
import os,pathlib,subprocess,tempfile
here=pathlib.Path(__file__).resolve().parent
w=pathlib.Path(tempfile.mkdtemp(prefix='dac-restore-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(w,flush=True)
env=dict(os.environ,DOSBOX_TEST_DAC='1',DOSBOX_TEST_CAPTURE_IP='0814:0200',DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0201')
p=subprocess.run(['python3',str(here/'stopped-save.py')],env=env,capture_output=True,text=True)
(w/'producer.log').write_text(p.stdout+p.stderr);p.check_returncode()
base=pathlib.Path(p.stdout.splitlines()[0])
env.update(DOSBOX_TEST_ARCHIVE=str(base/'first.sav'),DOSBOX_TEST_STEP_IP='0814:0201',DOSBOX_TEST_ROOT_EXIT='1',DOSBOX_TEST_ROOT_FACTS='code=42 resident=0 self-parent=0')
p=subprocess.run(['python3',str(here/'fresh-startup.py')],env=env,capture_output=True,text=True)
(w/'restore.log').write_text(p.stdout+p.stderr);p.check_returncode()
print('Actual DAC two components captured -> fresh held -> explicit third component -> guest RGB readback/root42 PASS')

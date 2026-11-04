#!/usr/bin/env python3
"""Loaded ROM identity through actual fresh-startup success/disposal consumers.

Matching different ROM paths are allowed; changed installed bytes are refused.
Producer also changes its pathname contents after initialization to prove that
capture describes loaded bytes, not a later reopen. No used-target load oracle.
"""
import os
import pathlib
import subprocess
import tempfile

here = pathlib.Path(__file__).resolve().parent
base = pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'state-inputs'
base.mkdir(exist_ok=True)
root = pathlib.Path(tempfile.mkdtemp(prefix='run-', dir=base))
producer = subprocess.run(
    ['python3', str(here / 'stopped-save.py')],
    env=dict(os.environ, DOSBOX_TEST_BASIC_ROM='1',
             DOSBOX_TEST_PROGRAM_HEX='90 b8 2a 4c cd 21 eb fe',
             DOSBOX_TEST_AUDIO='1', DOSBOX_TEST_LIVE_REFUSAL='1'),
    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
(root / 'producer.log').write_text(producer.stdout)
producer.check_returncode()
archive = pathlib.Path(producer.stdout.splitlines()[0]) / 'first.sav'
assert archive.is_file(), producer.stdout
rom = root / 'same-loaded-input.rom'
rom.write_bytes(bytes([0x90]) * 32768)
consumer = dict(os.environ, DOSBOX_TEST_ARCHIVE=str(archive),
                DOSBOX_TEST_AUDIO='1', DOSBOX_TEST_ROOT_EXIT='1',
                DOSBOX_TEST_BASIC_ROM='1', DOSBOX_TEST_BASIC_ROM_PATH=str(rom))
subprocess.run(['python3', str(here / 'fresh-startup.py')], env=consumer, check=True)
subprocess.run(['python3', str(here / 'fresh-startup-failure.py')],
               env=dict(consumer, DOSBOX_TEST_CHANGED_BASIC_ROM='1'), check=True)
print('Loaded immutable ROM identity: different-path success, changed-byte disposal passed')

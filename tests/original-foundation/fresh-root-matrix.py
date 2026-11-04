#!/usr/bin/env python3
"""Reproduce actual fresh root/child/TSR/self-parent cuts, never live reload.

DOSBOX_TEST_BINARY is the producer; DOSBOX_TEST_CONSUMERS is an optional
os.pathsep-separated list of compatible engine/ABI binaries with matching
hardware. Each existing probe owns its fixtures and assertions. Full subprocess
logs remain disposable evidence, not versioned goldens.
"""
import os
import pathlib
import subprocess
import tempfile

here = pathlib.Path(__file__).resolve().parent
root = pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'fresh-root-matrix'
root.mkdir(exist_ok=True)
work = pathlib.Path(tempfile.mkdtemp(prefix='run-', dir=root))
producer = os.environ['DOSBOX_TEST_BINARY']
consumers = os.environ.get('DOSBOX_TEST_CONSUMERS', producer).split(os.pathsep)
assert all(consumers)
# Prefix steps are explicit guest execution in the producer, before capture.
cases = (
    # Native service consumers after explicit continuation, not active-device
    # capture, waveform, rendered-frame or physical-input parity claims.
    ('speaker-timer', {'DOSBOX_TEST_SPEAKER': '1'},
     '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('video-service', {'DOSBOX_TEST_VIDEO': '1'},
     '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('keyboard-service', {'DOSBOX_TEST_KEYBOARD': '1'},
     '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('search-held', {'DOSBOX_TEST_SEARCH_HELD': '1',
                     'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0202', 'code=42 resident=0 self-parent=0'),
    ('search-matching', {'DOSBOX_TEST_SEARCH_HELD': '1',
                         'DOSBOX_TEST_SEARCH_MATCHING_ENTRY': '1',
                         'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0202', 'code=42 resident=0 self-parent=0'),
    ('search-deleted', {'DOSBOX_TEST_SEARCH_HELD': '1',
                        'DOSBOX_TEST_SEARCH_DELETED_ENTRIES': '1',
                        'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0202', 'code=42 resident=0 self-parent=0'),
    ('shared-file', {'DOSBOX_TEST_SHARED_FILE': '1',
                     'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0204', 'code=42 resident=0 self-parent=0'),
    ('xms-held', {'DOSBOX_TEST_XMS_HELD': '1',
                  'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0202', 'code=42 resident=0 self-parent=0'),
    ('ems-held', {'DOSBOX_TEST_EMS_HELD': '1',
                  'DOSBOX_TEST_CAPTURE_IP': '0814:0200'},
     '0814:0207', 'code=42 resident=0 self-parent=0'),
    ('child', {'DOSBOX_TEST_EXEC_CHILD': '1'}, '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('video-active', {'DOSBOX_TEST_VIDEO':'1','DOSBOX_TEST_VIDEO_ACTIVE':'1',
                       'DOSBOX_TEST_CAPTURE_IP':'0814:0200'},
     '0814:0201','code=42 resident=0 self-parent=0'),
    ('keyboard-active', {'DOSBOX_TEST_KEYBOARD':'1','DOSBOX_TEST_KEYBOARD_ACTIVE':'1',
                          'DOSBOX_TEST_CAPTURE_IP':'0814:0200'},
     '0814:0201','code=42 resident=0 self-parent=0'),
    ('speaker-active', {'DOSBOX_TEST_SPEAKER':'1','DOSBOX_TEST_SPEAKER_ACTIVE':'1',
                         'DOSBOX_TEST_CAPTURE_IP':'0814:0200'},
     '0814:0201','code=42 resident=0 self-parent=0'),
    ('child-tsr', {'DOSBOX_TEST_EXEC_CHILD': '1', 'DOSBOX_TEST_CHILD_TSR':'1',
                    'DOSBOX_TEST_CHILD_HEX':'ba 20 00 b8 07 31 cd 21'},
     '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('int22', {'DOSBOX_TEST_CUSTOM_INT22': '1'}, '0814:0101', 'code=42 resident=0 self-parent=0'),
    ('int27', {'DOSBOX_TEST_INT27': '1'}, '0814:0101', 'code=0 resident=1 self-parent=0'),
    ('ah31', {'DOSBOX_TEST_PROGRAM_HEX': '90 ba 00 10 b8 2a 31 cd 21 eb fe'},
     '0814:0101', 'code=42 resident=1 self-parent=0'),
    ('self', {'DOSBOX_TEST_PROGRAM_HEX': 'c7 06 16 00 14 08 90 b8 2a 4c cd 21 eb fe',
              'DOSBOX_TEST_CAPTURE_STEPS': '1'},
     '0814:0107', 'code=42 resident=0 self-parent=1'),
)
# Do not inherit fixture selection, steps or oracles from an unrelated probe.
env = {key: value for key, value in os.environ.items()
       if not key.startswith('DOSBOX_TEST_')}
env['DOSBOX_TEST_AUDIO'] = '1'
# Consumer-only variation must survive the fixture-selection environment reset.
consumer_policy = ({'DOSBOX_TEST_CAPTURE_POLICY': '1'}
                   if os.environ.get('DOSBOX_TEST_CAPTURE_POLICY') else {})
print(work, flush=True)
for name, setup, ip, facts in cases:
    capture = subprocess.run(
        ['python3', str(here / 'stopped-save.py')],
        env=dict(env, **setup, DOSBOX_TEST_BINARY=producer,
                 DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=' + ip),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (work / (name + '-producer.log')).write_text(capture.stdout)
    capture.check_returncode()
    archive = pathlib.Path(capture.stdout.splitlines()[0]) / 'first.sav'
    assert archive.is_file(), archive
    if setup.get('DOSBOX_TEST_SEARCH_DELETED_ENTRIES'):
        # These two files belong to this producer fixture. Preserve the archive
        # and all other evidence; the restore must reopen the current directory.
        for entry in ('ONE.DAT', 'TWO.DAT'):
            (archive.parent / entry).unlink()
    for index, binary in enumerate(consumers):
        result = subprocess.run(
            ['python3', str(here / 'fresh-startup.py')],
            env=dict(env, **({'DOSBOX_TEST_SEARCH_MATCHING_ENTRY': '1'}
                             if setup.get('DOSBOX_TEST_SEARCH_MATCHING_ENTRY') else {}),
                     **consumer_policy,
                     DOSBOX_TEST_BINARY=binary,
                     DOSBOX_TEST_ARCHIVE=str(archive), DOSBOX_TEST_ROOT_EXIT='1',
                     DOSBOX_TEST_STEP_IP=ip, DOSBOX_TEST_ROOT_FACTS=facts),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (work / (name + '-consumer-' + str(index) + '.log')).write_text(result.stdout)
        result.check_returncode()
    print(name, 'passed', len(consumers), 'fresh consumers', flush=True)

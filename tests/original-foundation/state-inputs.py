#!/usr/bin/env python3
"""Same configured ROM path: real matching and refusing fresh consumers."""
import pathlib
source=pathlib.Path('tests/original-foundation/stopped-save.py').read_text()
source=source[:source.index('\nrun(False)')]
source=source.replace('def run(restore):','''def run(restore):
 (w/'BASIC.ROM').write_bytes(bytes([0x91 if restore=='changed' else 0x90])*32768)''')
source=source.replace('mcp_server={listener.getsockname()[1]}\\n','mcp_server={listener.getsockname()[1]}\\nibm rom basic={w}/BASIC.ROM\\n')
start=source.index('  if slot_mode:\n   import runpy')
end=source.index('\n finally:',start)
source=source[:start]+'''  request('before',f'STATEWRITE {d}/before.sav')
  if restore:
   mismatch=restore=='changed'
   with zipfile.ZipFile(w/'capture/before.sav') as old, zipfile.ZipFile(d/'before.sav') as new:
    assert (old.read('Config_Identity')!=new.read('Config_Identity'))==mismatch
    assert old.read('Build_Identity')==new.read('Build_Identity')
   request('load',f'STATEREAD {w}/capture/before.sav',expect_failure=mismatch)
   request('after',f'STATEWRITE {d}/after.sav')
   with zipfile.ZipFile(d/'before.sav') as before, zipfile.ZipFile(d/'after.sav') as after, zipfile.ZipFile(w/'capture/before.sav') as producer:
    expected=before if mismatch else producer
    for name in expected.namelist():
     if name=='CPU' and not mismatch:
      assert expected.read(name)[:cpu_prefix]==after.read(name)[:cpu_prefix],name
     elif name=='Memory' and not mismatch:
      assert expected.read(name)[memory_descriptor:]==after.read(name)[memory_descriptor:],name
     elif mismatch and name!='Time_Stamp': assert expected.read(name)==after.read(name),name
    # Cross-process nonarchitectural owner bytes are covered separately by
    # stopped-save; this control proves stopped CPU/RAM install, not pointer parity.
   print('ROM',restore,'PASS',w,flush=True)
''' +source[end:]+ '\nrun(False)\nrun("matching")\nrun("changed")\n'
exec(compile(source,__file__,'exec'))

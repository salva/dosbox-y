#!/usr/bin/env python3
"""Lazy font owner must refuse capture rather than certify observed glyphs."""
import pathlib
source=pathlib.Path('tests/original-foundation/stopped-save.py').read_text()
source=source[:source.index('\nrun()')]
source=source.replace('[autoexec]\\n','[dosv]\\ndosv=jp\\n[autoexec]\\n')
start=source.index('  if slot_mode:\n   import runpy')
end=source.index('\n finally:',start)
source=source[:start]+'''  destination=w/'unavailable.sav'
  request('unsafe-input-capture',f'STATEWRITE {destination}',expect_failure=True)
  assert not destination.exists()
  assert not list(w.glob(destination.name+'.*'))
  request('still-stopped','SR BX 5678')
  print('lazy font input admission refusal PASS',w,flush=True)
'''+source[end:]+'\nrun()\n'
exec(compile(source,__file__,'exec'))

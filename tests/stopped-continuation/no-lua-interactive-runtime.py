"""Actual no-Lua terminal behavior; reuses native process/PTY resource fixture."""
from pathlib import Path
import runpy

# Reuse the real fixture without rewriting its Python source.
fixture = runpy.run_path(str(Path(__file__).parent / 'exclusive-runtime.py'))
fixture['run'](False, no_lua=True)
print('no-Lua Interactive commands explicit unavailable and native steps PASS')

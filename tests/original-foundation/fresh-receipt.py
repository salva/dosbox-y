#!/usr/bin/env python3
"""Record exact author inputs for an already completed fresh-root matrix.

Usage: fresh-receipt.py MATRIX_DIRECTORY OUTPUT_JSON
This records inputs, not success: check the matrix terminal result separately.
Runtime artifacts remain in card work; no archive bytes are copied into Git.
"""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

source = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
matrix = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()

def command(*args, cwd=source):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

tracked = command('git', 'ls-files', '-z').split('\0')
inputs = {name: digest(source / name) for name in tracked if name and
          (source / name).is_file() and not (source / name).is_symlink()}
# The successor may still be an authored untracked input when recording.
for path in (source / 'docs/PLANS').glob('FRESH-RESTORE-R2-SUCCESSOR-*.md'):
    inputs[str(path.relative_to(source))] = digest(path)
builds = {}
for name in ('no-agent', 'no-lua', 'lua'):
    directory = work / 'build' / name
    builds[name] = {file: digest(directory / file) for file in
                    ('config.h', 'config.status', 'Makefile', 'src/dosbox-x')}
    builds[name]['configuration'] = command(str(directory / 'config.status'),
                                           '--config', cwd=directory)
fixtures = {}
for log in sorted(matrix.glob('*-producer.log')):
    directory = Path(log.read_text().splitlines()[0])
    fixtures[log.name] = {
        'directory': str(directory),
        'files': {str(path.relative_to(directory)): digest(path)
                  for path in sorted(directory.rglob('*')) if path.is_file()}}
assert fixtures, 'no producer fixtures found'
receipt = {
    'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'status': 'author input receipt; not independent acceptance',
    'head': command('git', 'rev-parse', 'HEAD'),
    'tree': command('git', 'rev-parse', 'HEAD^{tree}'),
    'worktree': command('git', 'status', '--short'),
    'sources': inputs, 'builds': builds, 'fixtures': fixtures,
    'matrix': str(matrix),
    'matrix_logs': {path.name: digest(path) for path in sorted(matrix.glob('*.log'))},
    'toolchain': {tool: command(tool, '--version').splitlines()[0]
                  for tool in ('c++', 'make', 'nasm')},
}
output.write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')
print(output, digest(output))

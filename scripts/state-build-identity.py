#!/usr/bin/env python3
"""Content-stable exact archive build identity for configured release exports."""
import hashlib
import pathlib
import sys

source, build, output = map(pathlib.Path, sys.argv[1:])
digest = hashlib.sha256()


def add(name, data):
    for part in (name.encode(), data):
        digest.update(len(part).to_bytes(8, 'big'))
        digest.update(part)


for directory in ('src', 'include', 'vs'):
    for path in sorted((source / directory).rglob('*')):
        if path.is_file() and path.suffix in ('.cpp', '.c', '.h', '.hpp', '.in', '.am', '.S', '.asm', '.inc', '.cc', '.hh'):
            if path.name in ('config.h', 'build_timestamp.h', 'state_build_identity.h'):
                continue
            add(path.relative_to(source).as_posix(), path.read_bytes())
for name in ('config.h', 'config.status', 'src/Makefile', 'src/misc/Makefile'):
    add(name, (build / name).read_bytes())
add('generator', pathlib.Path(__file__).read_bytes())
text = '#define STATE_BUILD_ID "' + digest.hexdigest() + '"\n'
if not output.exists() or output.read_text() != text:
    output.write_text(text)

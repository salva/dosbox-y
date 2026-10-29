#!/usr/bin/env python3
"""Reproduce native minizip close/CRC settlement outside the emulator."""
import os,pathlib,struct,subprocess,tempfile,zipfile
source=pathlib.Path(__file__).resolve().parents[2]
work=pathlib.Path(tempfile.mkdtemp(prefix='minizip-close-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
archive=work/'input.zip'
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_STORED) as z:
 z.writestr('bad',b'abc')
image=bytearray(archive.read_bytes())
central=image.index(b'PK\x01\x02')
struct.pack_into('<I',image,central+16,0)
struct.pack_into('<I',image,14,0)
archive.write_bytes(image)
subprocess.run(['cc','-I.','-Ivs/zlib/contrib/minizip','tests/original-foundation/minizip-close.c','vs/zlib/contrib/minizip/unzip.c','vs/zlib/contrib/minizip/ioapi.c','-lz','-o',str(work/'check')],cwd=source,check=True)
subprocess.run([str(work/'check'),str(archive)],check=True)
print(work)

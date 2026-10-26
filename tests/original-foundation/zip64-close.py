#!/usr/bin/env python3
"""Real minizip ZIP64 finalization must retain output failures."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'zip64-close'
w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(r'''
#include <cassert>
#include <cstdio>
#include <cstdint>
#include "vs/zlib/contrib/minizip/zip.c"
static uint32_t rejected;
static int hits, closes;
static uLong ZCALLBACK write_checked(voidpf, voidpf stream, const void *buf, uLong size) {
 const auto *p = static_cast<const unsigned char*>(buf);
 if (rejected && size == 4 && (uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24) == rejected) {
  ++hits; return 0;
 }
 return static_cast<uLong>(fwrite(buf,1,size,static_cast<FILE*>(stream)));
}
static int ZCALLBACK close_checked(voidpf, voidpf stream) {
 ++closes; return fclose(static_cast<FILE*>(stream));
}
int main(int argc, char **argv) {
 assert(argc == 2);
 const uint32_t signatures[] = {0, 0x06064b50, 0x07064b50, 0x06054b50};
 for (auto signature: signatures) {
  rejected=signature; hits=closes=0;
  zlib_filefunc64_def io; fill_fopen64_filefunc(&io);
  io.zwrite_file=write_checked; io.zclose_file=close_checked;
  zipFile file=zipOpen2_64(argv[1],APPEND_STATUS_CREATE,nullptr,&io); assert(file);
  // Reach ZIP64 without a multi-gigabyte fixture. Exercise real finalization,
  // not a substitute serializer or mocked close-result assertion.
  static_cast<zip64_internal*>(file)->number_entry=0xffff;
  const int result=zipClose(file,nullptr);
  assert(result == (signature ? ZIP_ERRNO : ZIP_OK));
  assert(hits == (signature ? 1 : 0));
  assert(closes == 1);
 }
}
''')
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root),'-I'+str(root/'vs/zlib/contrib/minizip'),str(w/'test.cpp'),str(root/'vs/zlib/contrib/minizip/ioapi.c'),'-lz','-o',str(w/'test')],check=True)
subprocess.run([str(w/'test'),str(w/'archive.zip')],check=True)
print('ZIP64 record/locator/end write failure retained, actual native close exactly once: ASan/UBSan PASS')

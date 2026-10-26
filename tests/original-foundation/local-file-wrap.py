#!/usr/bin/env python3
"""Native reopen wrapper failure releases its still-owned descriptor exactly once."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/drive_local.cpp').read_text()
a = s.index('        hand = fdopen(fd, (flags&0xf)==OPEN_WRITE?')
b = s.index('\n#endif', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'local-file-wrap'
w.mkdir(exist_ok=True)
code = r'''
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#define _HT(x) x
constexpr unsigned OPEN_WRITE=1;
static bool fail=false;
static FILE* wrap(int fd,const char*mode) {
 if (fail) { errno=ENOMEM; return nullptr; }
 return fdopen(fd,mode);
}
#define fdopen wrap
bool reopen(int fd,FILE*&hand) {
 unsigned flags=0; const char*type="r+";
SEAM
 return true;
}
int main() {
 for (bool failure:{false,true}) {
  FILE* original=tmpfile();assert(original);
  int fd=dup(fileno(original));assert(fd>=0);
  FILE* hand=nullptr;fail=failure;
  assert(reopen(fd,hand)==!failure);
  if (failure) {
   assert(!hand && errno==ENOMEM);
   assert(fcntl(fd,F_GETFD)==-1 && errno==EBADF);
   // Reuse must not be affected by any subsequent cleanup of failed wrapping.
   int reused=dup(fileno(original));assert(reused==fd);
   assert(fcntl(reused,F_GETFD)>=0);assert(close(reused)==0);
  } else {
   assert(hand && fcntl(fd,F_GETFD)>=0);
   assert(fclose(hand)==0);
   assert(fcntl(fd,F_GETFD)==-1 && errno==EBADF);
  }
  assert(fclose(original)==0);
 }
}
'''.replace('SEAM',s[a:b]).replace('#include <cassert>', '#include <cassert>\n#include <initializer_list>')
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
# Windows CRT owns the OS handle after _open_osfhandle; _wfdopen failure
# must release that CRT descriptor, not CloseHandle the transferred handle.
wa = s.index('        hand = _wfdopen(nHandle, (flags&0xf)==OPEN_WRITE?')
wb = s.index('\n#else', wa)
windows = s[wa:wb].replace('_wfdopen', 'wrap').replace('_close', 'close').replace('nHandle', 'fd')
windows_code = code.replace(s[a:b], windows)
(w/'windows-ownership.cpp').write_text(windows_code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'windows-ownership.cpp'),'-o',str(w/'windows-ownership')],check=True)
subprocess.run([str(w/'windows-ownership')],check=True)
print('Actual POSIX and Windows CRT wrapping blocks descriptor ownership ASan/UBSan PASS; Windows native build unqualified')

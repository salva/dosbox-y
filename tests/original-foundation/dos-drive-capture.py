#!/usr/bin/env python3
"""Actual fixed description capture: bounds, padding and no write on refusal."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/dos/dos_files.cpp').read_text()
a=s.index('template <size_t N>\nstatic bool DOS_SaveStateCopyString')
b=s.index('void POD_Save_DOS_Files',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-drive-capture'
w.mkdir(exist_ok=True)
code='''#include <sstream>
#include <cstring>
#include <algorithm>
#include <cassert>
'''+s[a:b]+r'''
template<size_t N> void test(){
 for(size_t len: {size_t(0),N-1,N,N+99}){
  char dest[N];std::fill(dest,dest+N,'X');std::string source(len,'a');std::ostringstream out;
  bool ok=DOS_SaveStateCopyString(out,dest,source.c_str());assert(ok==(len<N));assert(bool(out)==ok);
  for(size_t i=0;i<N;++i)assert(dest[i]==(ok?(i<len?'a':'\0'):'X'));
 }
}
int main(){test<256>();test<512>();}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('DOS fixed capture bounds/padding/no-publication ASan/UBSan PASS')

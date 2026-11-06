#!/usr/bin/env python3
"""Exercise actual consumed TempLine extent checks/allocation with sanitizers."""
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/'src/hardware/vga_draw.cpp').read_text()
a=src.index('\tunsigned int savedTempLineSize = 0;')
b=src.index('\n\n',src.index('\tREAD_POD_SIZE( TempLine, savedTempLineSize );',a))
body=src[a:b]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-buffer'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'test.cpp').write_text('''#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <string>
uint8_t *TempLine=nullptr;unsigned TempLineSize=0;bool fail_alloc=false;
void TempLineFree(){free(TempLine);TempLine=nullptr;}
bool TempLineAlloc(unsigned w){if(fail_alloc)return false;TempLineSize=w*4+1024;TempLine=(uint8_t*)calloc(1,TempLineSize);return TempLine;}
#define READ_POD(p,x) stream.read(reinterpret_cast<char*>(p),sizeof(x))
#define READ_POD_SIZE(p,n) stream.read(reinterpret_cast<char*>(p),n)
void load(std::istream &stream){
'''+body+'''
}
std::stringstream input(unsigned n,bool full=true){std::string s(reinterpret_cast<char*>(&n),sizeof(n));if(full && n<=33792)s+=std::string(n,'x');return std::stringstream(s);}
int main(){
 for(unsigned n:{0u,1u,1023u,1025u,33793u,0xffffffffu}){auto s=input(n);load(s);assert(!s && !TempLine);}
 for(unsigned n:{1024u,2304u,33792u}){auto s=input(n);load(s);assert(s && TempLineSize==n && TempLine[n-1]=='x');}
 auto short_s=input(2304,false);load(short_s);assert(!short_s);
 fail_alloc=true;auto noalloc=input(1024);load(noalloc);assert(!noalloc && !TempLine);
 TempLineFree();
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Actual TempLine extent/truncation/allocation failure checks ASan/UBSan PASS')

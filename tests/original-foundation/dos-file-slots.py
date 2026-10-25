#!/usr/bin/env python3
"""Actual consumed slot tag/refcount, not device-identity admission."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-file-slots'
w.mkdir(exist_ok=True)
s=(root/'src/dos/dos_files.cpp').read_text()
a=s.index('\t\t\tuint8_t file_valid = 0;',s.index('void POD_Load_DOS_Files'))
b=s.index('\n\t\t\t// Detach any live',a)
code=r'''
#include <sstream>
#include <cstdint>
#include <cassert>
using Bits=long;
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
void E_Exit(const char*){throw 1;}
struct File{Bits refCtr=7;};
void load(std::istream&stream,File**Files){for(unsigned lcv=0;lcv<1;++lcv){
SEAM
(void)file_name;(void)file_namelen;(void)file_drive;(void)file_flags;
}}
int main(){for(unsigned tag=0;tag<256;++tag){
 File f;File *files[]={&f};std::string data(1,char(tag));Bits refs=12;
 data.append(reinterpret_cast<char*>(&refs),sizeof(refs));std::istringstream in(data);bool fatal=false;
 try{load(in,files);}catch(int){fatal=true;}
 assert(fatal==(tag!=0 && tag!=0xfe && tag!=0xff));assert(f.refCtr==(tag==0xfe?12:7));
 }
 for(size_t n=0;n<1+sizeof(Bits);++n){File f;File *files[]={&f};std::string data(1,char(0xfe));Bits refs=12;
 data.append(reinterpret_cast<char*>(&refs),sizeof(refs));std::istringstream in(data.substr(0,n));bool fatal=false;
 try{load(in,files);}catch(int){fatal=true;}assert(fatal && f.refCtr==7);}
 {File *files[]={nullptr};std::istringstream in(std::string(1,char(0xfe)));bool fatal=false;
 try{load(in,files);}catch(int){fatal=true;}assert(fatal);}
 {File f;File *files[]={&f};std::string data(1,char(0xfe));Bits refs=-1;data.append(reinterpret_cast<char*>(&refs),sizeof(refs));
 std::istringstream in(data);bool fatal=false;try{load(in,files);}catch(int){fatal=true;}assert(fatal && f.refCtr==7);}
}
'''.replace('SEAM',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DOS slot tags/null/truncation/refcounts ASan/UBSan PASS; not identity/resources')

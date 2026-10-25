#!/usr/bin/env python3
"""Actual DOS reopen-name capture/consumption seam, not drive/file lifetime."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-file-names'
w.mkdir(exist_ok=True)
s=(root/'src/dos/dos_files.cpp').read_text()
a=s.index('\t\t\tconst size_t name_size =')
b=s.index('\n\t\t\tfile_drive =',a)
c=s.index('\t\t\tREAD_POD( &file_namelen',s.index('void POD_Load_DOS_Files'))
d=s.index('\n\n',s.index('E_Exit("Invalid save-state DOS file name");',c))
code=r'''
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cassert>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
#define READ_POD_SIZE(p,n) stream.read(p,n)
void E_Exit(const char*){throw 1;}
struct File{char *name;};
void capture(std::ostream&stream,File **Files){unsigned lcv=0;uint8_t file_namelen;char *file_name;
CAPTURE
stream.write(reinterpret_cast<char*>(&file_namelen),1);stream.write(file_name,file_namelen);
}
void load(std::istream&stream){uint8_t file_namelen=0,file_drive=0,file_flags=0;char *file_name;
LOAD
}
int main(){
 for(size_t n:{size_t(0),size_t(254),size_t(255),size_t(4096)}){
 std::string name(n,'x');File f{&name[0]};File *files[]={&f};std::ostringstream out;capture(out,files);
 assert(bool(out)==(n<255));if(n<255){auto bytes=out.str();bytes.append(2,'\0');std::istringstream in(bytes);load(in);
 for(size_t k=0;k<bytes.size();++k){std::istringstream short_in(bytes.substr(0,k));bool fatal=false;
 try{load(short_in);}catch(int){fatal=true;}assert(fatal);}}
 }
 for(auto bad:{std::string(3,'\0'),std::string("\2xx\0\0",5),std::string("\3\0x\0\0\0",6)}){
 std::istringstream in(bad);bool fatal=false;try{load(in);}catch(int){fatal=true;}assert(fatal);}
}
'''.replace('CAPTURE',s[a:b]).replace('LOAD',s[c:d])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DOS name capture extents/truncation/termination ASan/UBSan PASS; not native resources')

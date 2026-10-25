#!/usr/bin/env python3
"""Real minizip streambuf roundtrip and explicit/destructor close lifetime."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'zip-stream';w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(r'''
#include <streambuf>
#include <ostream>
#include <istream>
#include <string>
#include <cassert>
#include <type_traits>
#include "zipcppstdbuf.h"
static_assert(!std::is_copy_constructible<zip_ostreambuf>::value,"exclusive ZIP member");
static_assert(!std::is_copy_constructible<zip_istreambuf>::value,"exclusive ZIP member");
int main(int argc,char **argv){
 assert(argc==2);zipFile z=zipOpen64(argv[1],APPEND_STATUS_CREATE);assert(z);
 const std::string value(100003,'x');
 for(int i=0;i<2;++i){
  assert(zipOpenNewFileInZip(z,i?"second":"first",nullptr,nullptr,0,nullptr,0,nullptr,Z_DEFLATED,6)==ZIP_OK);
  {zip_ostreambuf b(z);std::ostream s(&b);s << "header:" << 42 << '\n';s.put('!');s.write(value.data(),value.size());assert(s);
   assert(b.close()==ZIP_OK);assert(b.close()==ZIP_OK);}
 }
 assert(zipClose(z,nullptr)==ZIP_OK);
 unzFile u=unzOpen64(argv[1]);assert(u);
 for(int i=0;i<2;++i){
  assert(unzLocateFile(u,i?"second":"first",1)==UNZ_OK);assert(unzOpenCurrentFile(u)==UNZ_OK);
  {zip_istreambuf b(u);std::istream s(&b);const std::string expected="header:42\n!"+value;std::string got(expected.size(),' ');
   s.read(&got[0],got.size());assert(s && got==expected);
   char c;s.read(&c,1);assert(s.eof());assert(b.close()==UNZ_OK);assert(b.close()==UNZ_OK);}
 }
 assert(unzClose(u)==UNZ_OK);
}
''')
objects=[]
for name in ('zip','unzip','ioapi'):
 obj=w/(name+'.o');objects.append(str(obj))
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-I'+str(root/'include'),'-c',str(root/'vs/zlib/contrib/minizip'/(name+'.c')),'-o',str(obj)],check=True)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(w/'test.cpp')]+objects+['-lz','-o',str(w/'test')],check=True)
subprocess.run([str(w/'test'),str(w/'test.zip')],check=True)
print('real minizip streambuf two-member roundtrip/EOF/repeated close ASan/UBSan PASS')

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
#include <map>
#include <fstream>
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
   assert(s.peek()=='h');assert(s.peek()=='h');
   s.read(&got[0],got.size());assert(s && got==expected);
   char c;s.read(&c,1);assert(s.eof());assert(b.close()==UNZ_OK);assert(b.close()==UNZ_OK);}
 }
 assert(unzClose(u)==UNZ_OK);
 // Integrity pass uses real decompression and CRC, not directory metadata.
 u=unzOpen64(argv[1]);assert(u);
 assert(zip_verify_member(u,"first")==UNZ_OK);
 assert(zip_verify_member(u,"second")==UNZ_OK);
 assert(zip_verify_member(u,"SECOND",2)==UNZ_OK);
 assert(zip_verify_member(u,"SECOND")!=UNZ_OK);
 assert(zip_verify_member(u,"absent")!=UNZ_OK);
 assert(unzClose(u)==UNZ_OK);
 // Pure transport integrity; not all-owner preinstallation admission.
 {const char *names[]={"DOSBox-X_Version","Program_Name","Memory_Size",
                      "Machine_Type","Time_Stamp","Save_Remark","owner"};
  for(int trial=0;trial<3;++trial){
   z=zipOpen64(argv[1],APPEND_STATUS_CREATE);assert(z);
   for(const char *name:names){
    if(trial==1 && std::string(name)=="owner")continue;
    assert(zipOpenNewFileInZip(z,name,nullptr,nullptr,0,nullptr,0,nullptr,0,0)==ZIP_OK);
    {zip_ostreambuf b(z);const std::string payload=std::string(name)=="owner"?"owner-payload-marker":"metadata";
     assert(b.sputn(payload.data(),payload.size())==std::streamsize(payload.size()));assert(b.close()==ZIP_OK);}
   }
   assert(zipClose(z,nullptr)==ZIP_OK);
   if(trial==2){
    std::ifstream in(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
    const auto at=bytes.find("owner-payload-marker");assert(at!=std::string::npos);bytes[at]^=1;
    in.close();std::ofstream out(argv[1],std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());assert(out);
   }
   u=unzOpen64(argv[1]);assert(u);
   {unzFile zf=u;int err=UNZ_OK;bool load_err=false;
    using CompEntry=std::map<std::string,int>;CompEntry components{{"owner",0}};
    bool installed=false;
    auto load=[&](){
     for (const char *name:names) {
      err=zip_verify_member(zf,name);
      if(err!=UNZ_OK){load_err=true;return;}
     }
     installed=true;
     return;
    };
    load();assert(installed==(trial==0));assert(load_err==(trial!=0));
   }
   assert(unzClose(u)==UNZ_OK);
  }
 }
 // Stored member permits deterministic payload corruption without damaging
 // the central directory: CRC failure must survive repeat close/destruction.
 z=zipOpen64(argv[1],APPEND_STATUS_CREATE);assert(z);
 assert(zipOpenNewFileInZip(z,"crc",nullptr,nullptr,0,nullptr,0,nullptr,0,0)==ZIP_OK);
 const std::string marker="unique-payload-for-crc-check";
 {zip_ostreambuf b(z);assert(b.sputn(marker.data(),marker.size())==std::streamsize(marker.size()));assert(b.close()==ZIP_OK);}
 assert(zipClose(z,nullptr)==ZIP_OK);
 {std::ifstream in(argv[1],std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});
  const auto at=bytes.find(marker);assert(at!=std::string::npos);bytes[at]^=1;
  in.close();std::ofstream out(argv[1],std::ios::binary|std::ios::trunc);out.write(bytes.data(),bytes.size());assert(out);}
 u=unzOpen64(argv[1]);assert(u);
 assert(zip_verify_member(u,"crc")==UNZ_CRCERROR);
 // The actual flagged restore integrity branch must skip destination effects.
 {unzFile zf=u;int i=0,ret=0;const char *g_flagged_files[]={"CRC","absent"};
  bool created=false;
  auto restore=[&](){
   while (i<2) {
FLAGGED_INTEGRITY_SEAM
    created=true;
    ++i;
   }
  };
  restore();
  assert(ret==-1 && !created);
 }
 assert(unzClose(u)==UNZ_OK);
 u=unzOpen64(argv[1]);assert(u);assert(unzLocateFile(u,"crc",1)==UNZ_OK);assert(unzOpenCurrentFile(u)==UNZ_OK);
 {zip_istreambuf b(u);std::string bytes(marker.size(),' ');assert(b.sgetn(&bytes[0],bytes.size())==std::streamsize(bytes.size()));
  assert(b.close()==UNZ_CRCERROR);assert(b.close()==UNZ_CRCERROR);}
 assert(unzClose(u)==UNZ_OK);
 // Stream failure remains observable even when native ZIP close succeeds.
 z=zipOpen64(argv[1],APPEND_STATUS_CREATE);assert(z);
 assert(zipOpenNewFileInZip(z,"failed-owner",nullptr,nullptr,0,nullptr,0,nullptr,0,0)==ZIP_OK);
 {zip_ostreambuf zos(z);std::ostream ss(&zos);
  ss<<"partial";ss.setstate(std::ios::failbit);
  assert(!ss);assert(zos.close()==ZIP_OK);
 }
 assert(zipClose(z,nullptr)==ZIP_OK);
}
''')
source=w/'test.cpp'
flagged=(root/'src/dos/dos_programs.cpp').read_text()
a=flagged.index('\t\t\t\t\tif (zip_verify_member(zf, g_flagged_files[i],')
b=flagged.index('\n\t\t\t\t\tif (DOS_FindDevice',a)
source.write_text(source.read_text().replace('FLAGGED_INTEGRITY_SEAM',flagged[a:b]))
# Production metadata/owner completion is covered by progressive-install.py
# and state-archive-output.py, not a recreated global preflight.
objects=[]
for name in ('zip','unzip','ioapi'):
 obj=w/(name+'.o');objects.append(str(obj))
 subprocess.run(['cc','-fsanitize=address,undefined','-fno-pie','-I'+str(root/'include'),'-c',str(root/'vs/zlib/contrib/minizip'/(name+'.c')),'-o',str(obj)],check=True)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(w/'test.cpp')]+objects+['-lz','-o',str(w/'test')],check=True)
subprocess.run([str(w/'test'),str(w/'test.zip')],check=True)
print('real minizip streambuf two-member roundtrip/EOF/repeated close ASan/UBSan PASS')

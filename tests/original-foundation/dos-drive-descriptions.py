#!/usr/bin/env python3
"""Exercise consumed DOS drive/image tags and bounded descriptions, not resource admission."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text().split('void POD_Load_DOS_Files', 1)[1]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-drive-descriptions'
w.mkdir(exist_ok=True)
# Extract the actual read/validation spans, stopping before any native effect.
def span(start, end, offset=0):
    a = s.index(start, offset)
    return s[a:s.index(end, a)]
tag = span('uint8_t drive_valid = 0;', '\n\t\t\tif( drive_valid')
image = span('uint8_t valid = 0;', '\n        if (!valid)')
a = s.index('READ_POD( &dinfo, dinfo);')
desc = span('READ_POD( &dinfo, dinfo);', '\n#if !defined(WIN32)', a)
a = s.index('} else {', s.index('// Detach any live'))
short = span('READ_POD( &dinfo, dinfo);', '\n            if( Drives', a)
disk = span('READ_POD( &diskname, diskname );', '\n        Bitu sizes')
code = r'''
#include <sstream>
#include <cstring>
#include <cstdint>
#include <cassert>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
void E_Exit(const char*){throw 1;}
void tag(std::istream &stream){ TAG }
void image(std::istream &stream){ IMAGE }
void desc(std::istream &stream){char dinfo[256],overlaydir[512];uint32_t lalloc,oalloc,opts; DESC }
void small(std::istream &stream){char dinfo[256]; SHORT }
void disk(std::istream &stream){char diskname[256];uint32_t opts; DISK }
bool bad(void(*f)(std::istream&),const std::string &s){std::istringstream in(s);try{f(in);}catch(int){return true;}return false;}
int main(){
 for(unsigned n=0;n<256;++n){std::string s(1,char(n));assert(bad(tag,s)==(n!=0 && n!=255));assert(bad(image,s)==(n>1));}
 assert(bad(tag,""));assert(bad(image,""));
 struct Case{void(*f)(std::istream&);size_t size;};
 for(auto c:{Case{desc,780},Case{small,256},Case{disk,260}}){
   std::string valid(c.size,'\0');assert(!bad(c.f,valid));
   for(size_t n=0;n<c.size;++n)assert(bad(c.f,valid.substr(0,n)));
   std::string invalid=valid;invalid.replace(0,256,256,'x');assert(bad(c.f,invalid));
   if(c.f==desc){invalid=valid;invalid.replace(256,512,512,'x');assert(bad(c.f,invalid));}
 }
}
'''
for key, value in [('TAG',tag),('IMAGE',image),('DESC',desc),('SHORT',short),('DISK',disk)]:
    code = code.replace(key, value)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('DOS drive/image consumed tag/description ASan/UBSan PASS; not native-resource preparation')

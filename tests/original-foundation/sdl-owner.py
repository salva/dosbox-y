#!/usr/bin/env python3
"""Original SDL owner: complete reads and valid bools before publication.

This is owner-local validation, not portable input reconstruction or archive-wide
preflight. Host acquisition flags remain host state, not a safe native save join.
"""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'sdl-owner-regression'
work.mkdir(exist_ok=True)
source = (root / 'src/gui/sdlmain.cpp').read_text()
start = source.index('void POD_Save_Sdlmain(')
body = source[start:]
# These are the final functions in this source; no substitute decoder is used.
fixture = r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#define WRITE_POD(x,y) stream.write(reinterpret_cast<const char*>(x),sizeof(y))
struct {struct {bool autolock=false, requestlock=false;} mouse;} sdl;
'''+body+r'''
int main(){
 for(bool a:{false,true})for(bool b:{false,true}){
  sdl.mouse.autolock=a;sdl.mouse.requestlock=b;
  std::ostringstream out;POD_Save_Sdlmain(out);const auto bytes=out.str();
  for(size_t n=0;n<bytes.size();++n){
   sdl.mouse.autolock=!a;sdl.mouse.requestlock=!b;
   std::istringstream in(bytes.substr(0,n));POD_Load_Sdlmain(in);
   assert(in.fail() && sdl.mouse.autolock==!a && sdl.mouse.requestlock==!b);
   std::istringstream throwing(bytes.substr(0,n));
   throwing.exceptions(std::ios::failbit|std::ios::badbit);
   bool caught=false;try{POD_Load_Sdlmain(throwing);}catch(const std::ios_base::failure&){caught=true;}
   assert(caught && sdl.mouse.autolock==!a && sdl.mouse.requestlock==!b);
  }
  std::istringstream in(bytes);POD_Load_Sdlmain(in);
  assert(in && sdl.mouse.autolock==a && sdl.mouse.requestlock==b);
 }
 // Exhaust every byte for both serialized flags on the tested one-byte ABI.
 static_assert(sizeof(bool)==1,"fixture enumerates one-byte bool ABI");
 for(unsigned value=2;value<256;++value)for(unsigned field=0;field<2;++field){
  sdl.mouse.autolock=true;sdl.mouse.requestlock=false;
  std::string bytes(2,'\0');bytes[field]=static_cast<char>(value);
  std::istringstream in(bytes);POD_Load_Sdlmain(in);
  assert(in.fail() && sdl.mouse.autolock && !sdl.mouse.requestlock);
  std::istringstream throwing(bytes);throwing.exceptions(std::ios::failbit);
  bool caught=false;try{POD_Load_Sdlmain(throwing);}catch(const std::ios_base::failure&){caught=true;}
  assert(caught && sdl.mouse.autolock && !sdl.mouse.requestlock);
 }
}
'''
(work / 'test.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-pie','-no-pie',
                str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('SDL original owner short-read/invalid-bool ASan/UBSan PASS; not native save qualification')

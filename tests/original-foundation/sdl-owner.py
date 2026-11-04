#!/usr/bin/env python3
"""Actual SDL owner excludes host acquisition policy; no backend runtime claim."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'sdl-owner-regression'
work.mkdir(exist_ok=True)
source=(root/'src/gui/sdlmain.cpp').read_text()
body=source[source.index('void POD_Save_Sdlmain('):]
fixture=r'''
#include <cassert>
#include <sstream>
#include <string>
struct {struct {bool autolock=false, requestlock=false;} mouse;} sdl;
'''+body+r'''
int main(){
 for(bool a:{false,true})for(bool b:{false,true}){
  sdl.mouse.autolock=a;sdl.mouse.requestlock=b;
  std::ostringstream out;POD_Save_Sdlmain(out);assert(out && out.str().empty());
  assert(sdl.mouse.autolock==a && sdl.mouse.requestlock==b);
  std::istringstream in("");POD_Load_Sdlmain(in);
  assert(in && sdl.mouse.autolock==a && sdl.mouse.requestlock==b);
  // Legacy/host flags are not consumed. The common member EOF check rejects
  // trailing bytes rather than interpreting them as host acquisition grants.
  std::istringstream old(std::string(2,'\1'));POD_Load_Sdlmain(old);
  assert(old.peek()==1 && sdl.mouse.autolock==a && sdl.mouse.requestlock==b);
  std::istringstream failed;failed.setstate(std::ios::badbit);POD_Load_Sdlmain(failed);
  assert(failed.bad() && sdl.mouse.autolock==a && sdl.mouse.requestlock==b);
 }
}
'''
(work/'test.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual SDL owner: empty host-neutral payload preserves destination acquisition flags; ASan/UBSan, not backend runtime proof')

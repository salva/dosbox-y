#!/usr/bin/env python3
"""Actual mixer traversal with stub chip codecs: stop on first failed owner."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mixer-consumption'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/hardware/mixer.cpp').read_text()
codec='namespace {\n'+t[t.index('class SerializeMixer'):]
layout=t[t.index('static struct {'):t.index('} mixer;')+len('} mixer;')].replace('static struct','struct')
layout=layout.replace('MixerChannel*   channels','void*   channels')
source=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
using Bitu=uintptr_t;
#define MIXER_BUFSIZE 16384
struct mixedFraction {unsigned w,fn,fd;};
struct MixerStateLock {MixerStateLock(){} ~MixerStateLock(){}};
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
'''+layout+r'''
unsigned visits=0;
#define CHIP(name) void POD_Save_##name(std::ostream &s){s.put('x');} \
 void POD_Load_##name(std::istream &s){++visits;if(s.get()!='x')s.setstate(std::ios::failbit);}
CHIP(Adlib) CHIP(Disney) CHIP(Gameblaster) CHIP(GUS) CHIP(MPU401) CHIP(PCSpeaker) CHIP(Sblaster) CHIP(Tandy_Sound)
'''+codec+r'''
int main(){
 mixer.pos=77;mixer.done=88;
 std::ostringstream capture;SaveState::instance().owner->getBytes(capture);
 auto bytes=capture.str();assert(capture && bytes.size()>8);
 for(unsigned i=0;i<8;++i){
  auto bad=bytes;bad[bad.size()-8+i]='!';visits=0;mixer.pos=77;mixer.done=88;
  std::istringstream s(bad);SaveState::instance().owner->setBytes(s);
  assert(s.fail() && visits==i+1 && mixer.pos==77 && mixer.done==88);
 }
 std::istringstream s(bytes);visits=0;
 SaveState::instance().owner->setBytes(s);
 assert(s && visits==8 && mixer.pos==77 && mixer.done==88);
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Mixer traversal: eight failed-owner cuts and successful logical cursor continuation passed')

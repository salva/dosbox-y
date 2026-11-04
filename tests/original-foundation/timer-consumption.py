#!/usr/bin/env python3
"""Actual PIT layout/codec and time consumers, reduced native dependencies."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'timer-consumption'
work.mkdir(exist_ok=True)
header=(root/'include/dosbox.h').read_text()
pod=header[header.index('class SerializeGlobalPOD :'):header.index('//---------------- inline implementation')]
text=(root/'src/hardware/timer.cpp').read_text()
start=text.index('struct PIT_Block {')
end=text.index('pic_tickindex_t VGA_PITSync_delay')
layout=text[start:end]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>
using Bitu=unsigned long;
using pic_tickindex_t=double;
extern unsigned long PIT_TICK_RATE;
constexpr unsigned long PIT_TICK_RATE_IBM=1193182;
double pic_tickfmod(double x,double y){return fmod(x,y);}
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+layout+text[text.index('namespace\n{\nclass SerializeTimer'):]+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 for(auto &c:pit){c.cntr=c.cntr_cur=0x10000;c.delay=54.9254;c.start=100;c.now=102;c.mode=3;c.gate=true;}
 const auto before=pit[0].read_counter();const auto bytes=save();pit[0].start=0;
 assert(load(bytes));assert(pit[0].start==100 && pit[0].now==102);
 const auto after=pit[0].read_counter();assert(before.counter==after.counter && before.cycle==after.cycle);
 for(size_t n=0;n<bytes.size();++n)assert(!load(bytes.substr(0,n)));
 assert(load(bytes));pit[0].mode=6;assert(!load(save()));
 assert(load(bytes));pit[1].read_state=4;assert(!load(save()));
 assert(load(bytes));pit[1].write_state=static_cast<PIT_Block::WriteState>(4);assert(!load(save()));
 assert(load(bytes));pit[2].cntr_cur=0x10001;assert(!load(save()));
 assert(load(bytes));pit[2].delay=INFINITY;assert(!load(save()));
 assert(load(bytes));pit[2].start=NAN;assert(!load(save()));
 assert(load(bytes));pit[2].delay=0;assert(!load(save()));
 assert(load(bytes));pit[2].cntr_cur=0;assert(!load(save()));
 auto bad=bytes;bad[sizeof(pit[0].latched_timerstatus)]=2;assert(!load(bad));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('PIT codec: truncations, bool/mode/counter/time bounds, unchanged saved clock and counter consumer passed')

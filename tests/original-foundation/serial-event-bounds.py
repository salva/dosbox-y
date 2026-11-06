#!/usr/bin/env python3
"""Actual serial event dispatch body: invalid low nibbles never index ports."""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
src = (root/'src/hardware/serialport/serialport.cpp').read_text()
a = src.index('static void Serial_EventHandler(Bitu val) {')
b = src.index('\nvoid CSerial::setEvent', a)
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'serial-event-bounds'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
    p = Path(d)
    (p/'probe.cpp').write_text('''#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;
struct Port { unsigned calls=0; uint16_t last=0;
 void handleEvent(uint16_t type) { ++calls; last=type; }
};
Port* serialports[9]{};
'''+src[a:b]+'''
int main() {
 Port p;
 for (unsigned i=0;i<9;++i) serialports[i]=&p;
 for(unsigned type: {0u,1u,7u,65535u}) {
  for(unsigned id=0;id<16;++id) {
   auto before=p.calls;
   Serial_EventHandler((Bitu(type)<<4)|id);
   assert(p.calls==before+(id<9));
   if(id<9) assert(p.last==type);
  }
 }
 serialports[8]=nullptr;
 auto before=p.calls;Serial_EventHandler(8);assert(p.calls==before);
}
'''.replace('int main()', '#include <initializer_list>\nint main()'))
    subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
    subprocess.run([str(p/'probe')],check=True)
print('Serial_EventHandler actual-body ASan/UBSan valid/null/out-of-range dispatch PASS')

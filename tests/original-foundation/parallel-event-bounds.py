#!/usr/bin/env python3
"""Actual parallel event dispatch: invalid low nibbles never index ports."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=(root/'src/hardware/parport/parport.cpp').read_text()
a=src.index('static void Parallel_EventHandler(Bitu val) {')
b=src.index('\nvoid CParallel::setEvent',a)
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'parallel-event-bounds'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'probe.cpp').write_text('''#include <cassert>
#include <cstdint>
#include <initializer_list>
using Bitu=uintptr_t;
struct Port {unsigned calls=0;uint16_t last=0;void handleEvent(uint16_t t){++calls;last=t;}};
Port* parallelPortObjects[9]{};
'''+src[a:b]+'''
int main(){Port p;for(auto &v:parallelPortObjects)v=&p;
 for(unsigned type:{0u,1u,7u,65535u})for(unsigned id=0;id<16;++id){
  auto n=p.calls;Parallel_EventHandler((Bitu(type)<<4)|id);
  assert(p.calls==n+(id<9));if(id<9)assert(p.last==type);
 }
 parallelPortObjects[8]=nullptr;auto n=p.calls;Parallel_EventHandler(8);assert(p.calls==n);
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
 subprocess.run([str(p/'probe')],check=True)
print('Parallel_EventHandler actual-body ASan/UBSan valid/null/out-of-range dispatch PASS')

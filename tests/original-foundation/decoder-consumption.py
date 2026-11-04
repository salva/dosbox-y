#!/usr/bin/env python3
"""Real CPU decoder selection body with inert cache-owner reset effects."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'decoder-consumption'
work.mkdir(exist_ok=True)
text = (root / 'src/cpu/cpu.cpp').read_text()
def body(signature):
    start = text.index(signature)
    begin = text.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]
source = r'''
#include <cassert>
#include <cstdint>
#define C_DYNAMIC_X86 1
#define C_DYNREC 1
using CPU_Decoder=int();
int CPU_Core_Normal_Run(){return 0;}
int CPU_Core_Prefetch_Run(){return 1;}
int CPU_Core_Simple_Run(){return 2;}
int CPU_Core_Full_Run(){return 3;}
int CPU_Core_Dyn_X86_Run(){return 4;}
int CPU_Core_Dynrec_Run(){return 5;}
int CPU_Core_Normal_Trap_Run(){return 100;}
int CPU_Core_Dyn_X86_Trap_Run(){return 101;}
int CPU_Core_Dynrec_Trap_Run(){return 102;}
int HLT_Decode(){return 200;}
CPU_Decoder *cpudecoder=nullptr;
unsigned x86_resets=0,dynrec_resets=0;
void CPU_Core_Dyn_X86_Cache_Reset(){++x86_resets;}
void CPU_Core_Dynrec_Cache_Reset(){++dynrec_resets;}
'''
source += '\n'.join(body(s) for s in ['uint16_t CPU_FindDecoderType(', 'CPU_Decoder* CPU_IndexDecoderType(', 'static void selectDecoder('])
# Exercise the actual installation body with a reduced CPUBlock, not a
# substitute decoder assignment. No CPU/PIC/TIMER interpreter is invoked.
source += r'''
struct CPUBlock {struct{CPU_Decoder *old_decoder=nullptr;}hlt;} cpu;
int cpu_regs=0,Segs=0,CPU_Cycles=0,CPU_CycleLeft=0,CPU_IODelayRemoved=0,lastint=0;
struct TaskStateSegment{}cpu_tss;
'''
source += body('struct Pending {')+';\n'
source += r'''
int main() {
 const uint16_t modes[]={0,1,2,3,4,5,100,101,102,200};
 unsigned count=0;
 for(auto previous:modes)for(auto next:modes) {
  cpudecoder=CPU_IndexDecoderType(previous);selectDecoder(next);++count;
  assert(cpudecoder && CPU_FindDecoderType(cpudecoder)==next);
  assert(x86_resets==count && dynrec_resets==count);
 }
 assert(CPU_IndexDecoderType(99)==nullptr);
 assert(CPU_IndexDecoderType(0xffff)==nullptr);
 for(auto mode:modes)if(mode!=200){
  Pending saved{};saved.decoder=200;saved.hlt_decoder=mode;
  saved.cycles=123;saved.left=456;saved.io=789;
  cpu.hlt.old_decoder=CPU_IndexDecoderType(3);saved.install();
  assert(cpudecoder==CPU_IndexDecoderType(200));
  assert(cpu.hlt.old_decoder==CPU_IndexDecoderType(mode));
  assert(CPU_Cycles==123 && CPU_CycleLeft==456 && CPU_IODelayRemoved==789);
  saved.decoder=mode;saved.hlt_decoder=0xffff;saved.install();
  assert(cpudecoder==CPU_IndexDecoderType(mode) && cpu.hlt.old_decoder==nullptr);
 }
}
'''
path = work/'consumer.cpp'
path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('CPU decoder: all 100 mode transitions select saved decoder; native cache resets passed')

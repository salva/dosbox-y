#!/usr/bin/env python3
"""Exercise actual held/debug-running loop body with host and IRQ spies."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'held-loop'
work.mkdir(exist_ok=True)
text = (root / 'src/debug/debug.cpp').read_text()
start = text.index('Bitu DEBUG_Loop(void) {')
end = text.index('\nvoid DEBUG_FlushInput(void);', start)
source = r'''
#include <cassert>
#include <cstdint>
using Bitu = unsigned;
using LoopHandler = Bitu();
#define C_DYNAMIC_X86 0
static bool debug_running=false, check_rescroll=false;
static bool logBuffSuppressConsoleNeedUpdate=false;
static unsigned host=0, keys=0, irqs=0, draws=0, running=0, polls=0;
static Bitu DEBUG_LastRunningUpdate=0;
static uint16_t guest_cs=0x1234;
static uint32_t guest_ip=0x5678;
static Bitu guest_tick=123;
struct View {uint16_t useCS=0; uint32_t useEIP=0; int cursorPos=0;} codeViewData;
Bitu DEBUG_Loop();
static Bitu normal() {++running; return 17;}
static LoopHandler *current=DEBUG_Loop, *old_loop=normal;
static LoopHandler *DOSBOX_GetLoop() {return current;}
static void ControlServer_Poll() {++polls;}
static unsigned SDL_GetTicks() {return 100;}
static void GFX_Events() {++host;}
static void SDL_Delay(unsigned) {}
static void SetCodeWinStart() {codeViewData.cursorPos=0;}
static void DEBUG_DrawScreen() {++draws;}
static void DEBUG_RefreshPage(unsigned) {++draws;}
static Bitu DEBUG_CheckKeys() {++keys; return 19;}
// An IRQ spy would mutate architectural state if invoked while held.
[[maybe_unused]] static void PIC_runIRQs() {++irqs; ++guest_cs; ++guest_ip; ++guest_tick;}
'''
source += text[start:end]
source += r'''
int main() {
    for (unsigned i=0;i<100;++i) assert(DEBUG_Loop()==19);
    assert(host==100 && keys==100 && polls==100);
    assert(!irqs && !running && guest_cs==0x1234 && guest_ip==0x5678 && guest_tick==123);
    check_rescroll=true; codeViewData.cursorPos=7;
    logBuffSuppressConsoleNeedUpdate=true;
    assert(DEBUG_Loop()==19);
    assert(!check_rescroll && !logBuffSuppressConsoleNeedUpdate && draws==1);
    assert(codeViewData.cursorPos==7 && !irqs);
    debug_running=true;
    assert(DEBUG_Loop()==17 && running==1 && host==101 && keys==101);
    current=normal;
    assert(DEBUG_Loop()==0 && running==1 && host==101);
}
'''
(work / 'held.cpp').write_text(source)
subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-pie','-no-pie',
                str(work / 'held.cpp'),'-o',str(work / 'held')],check=True)
subprocess.run([str(work / 'held')],check=True)
print('actual debugger held-loop host service/no IRQ/implicit resume PASS; not live frontend qualification')

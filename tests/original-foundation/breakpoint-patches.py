#!/usr/bin/env python3
"""Actual non-heavy breakpoint owner, with bounded instruction-memory backing."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'breakpoint-patches'
work.mkdir(exist_ok=True)
text = (root / 'src/debug/debug.cpp').read_text()
def body(start):
    at = text.index(start)
    begin = text.index('{', at)
    depth, end = 1, begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[at:end]
source = r'''
#include <cstdint>
#include <list>
#include <cassert>
#include <stdexcept>
#include <cstdarg>
#define C_HEAVY_DEBUG 0
using PhysPt = uint32_t;
static uint8_t memory[65536];
static unsigned diagnostics = 0;
static unsigned writes = 0;
static PhysPt GetAddress(uint16_t seg, uint32_t off) {return seg * 16u + off;}
static uint8_t mem_readb(PhysPt addr) {assert(addr < sizeof memory); return memory[addr];}
static void mem_writeb(PhysPt addr, uint8_t value) {
    assert(addr < sizeof memory); memory[addr] = value; ++writes;
}
static void DEBUG_ShowMsg(const char *, ...) {++diagnostics;}
static void E_Exit(const char *) {throw std::runtime_error("fatal");}
'''
source += text[text.index('enum EBreakpoint'):text.index('CBreakpoint::CBreakpoint(void)')]
source += '\n'.join(body(x) for x in [
    'CBreakpoint::CBreakpoint(void)', 'void CBreakpoint::Activate(bool _active)',
    'CBreakpoint* CBreakpoint::AddBreakpoint(uint16_t seg',
    'void CBreakpoint::ActivateBreakpoints()', 'void CBreakpoint::DeactivateBreakpoints()',
    'bool CBreakpoint::CheckBreakpoint(uint16_t seg',
    'void CBreakpoint::DeleteAll()', 'bool CBreakpoint::DeleteByIndex(uint16_t index)',
    'CBreakpoint* CBreakpoint::FindPhysBreakpoint(uint16_t seg',
    'CBreakpoint* CBreakpoint::FindOtherActiveBreakpoint(PhysPt adr',
    'bool CBreakpoint::DeleteBreakpoint(uint16_t seg',
    'bool CBreakpoint::DeleteBreakpoint(CBreakpoint* breakpoint)',
    'bool CBreakpoint::DeleteById(uintptr_t id)',
    'uintptr_t CBreakpoint::ConsumeLastTriggered(void)'])
source += r'''
std::list<CBreakpoint*> CBreakpoint::BPoints;
uintptr_t CBreakpoint::lastTriggered = 0, CBreakpoint::next_id = 1;
int main() {
    // Each public deletion path must restore the actual instruction byte.
    for (unsigned route = 0; route < 4; ++route) {
        memory[0x1234] = 0x90;
        auto *bp = CBreakpoint::AddBreakpoint(0x120, 0x34, false);
        const auto id = bp->GetId();
        bp->Activate(true);
        assert(memory[0x1234] == 0xcc);
        if (route == 0) assert(CBreakpoint::DeleteBreakpoint(0x120, 0x34));
        if (route == 1) assert(CBreakpoint::DeleteByIndex(0));
        if (route == 2) assert(CBreakpoint::DeleteById(id));
        if (route == 3) CBreakpoint::DeleteAll();
        assert(memory[0x1234] == 0x90);
        assert(!CBreakpoint::DeleteById(id));
    }
    // Two native breakpoint objects at one physical byte share its old data.
    // Removing either first must leave the other's patch and original byte intact.
    for (bool reverse : {false, true}) {
        memory[0x1234] = 0x55;
        auto *a = CBreakpoint::AddBreakpoint(0x120, 0x34, false);
        auto *b = CBreakpoint::AddBreakpoint(0x123, 0x04, true);
        const auto a_id = a->GetId(), b_id = b->GetId();
        a->Activate(true); b->Activate(true);
        a->Activate(true); b->Activate(true);
        assert(memory[0x1234] == 0xcc);
        assert(CBreakpoint::DeleteById(reverse ? b_id : a_id));
        assert(memory[0x1234] == 0xcc);
        assert(CBreakpoint::DeleteById(reverse ? a_id : b_id));
        assert(memory[0x1234] == 0x55);
    }
    // A real one-shot hit retires the patch but retains its value receipt.
    memory[0x1234] = 0x40;
    auto *once = CBreakpoint::AddBreakpoint(0x120, 0x34, true);
    const auto once_id = once->GetId();
    once->Activate(true);
    assert(!CBreakpoint::CheckBreakpoint(0x120, 0x35));
    assert(CBreakpoint::CheckBreakpoint(0x120, 0x34));
    assert(memory[0x1234] == 0x40);
    assert(CBreakpoint::ConsumeLastTriggered() == once_id);
    assert(!CBreakpoint::ConsumeLastTriggered());
    assert(!CBreakpoint::DeleteById(once_id));
    // Permanent hit retires a co-located temporary, independent of list order.
    for (bool temporary_first : {false, true}) {
        memory[0x1234] = 0x48;
        CBreakpoint *permanent, *temporary;
        if (temporary_first) {
            temporary = CBreakpoint::AddBreakpoint(0x120, 0x34, true);
            permanent = CBreakpoint::AddBreakpoint(0x120, 0x34, false);
        } else {
            permanent = CBreakpoint::AddBreakpoint(0x120, 0x34, false);
            temporary = CBreakpoint::AddBreakpoint(0x120, 0x34, true);
        }
        const auto permanent_id = permanent->GetId(), temporary_id = temporary->GetId();
        CBreakpoint::ActivateBreakpoints();
        assert(CBreakpoint::CheckBreakpoint(0x120, 0x34));
        assert(CBreakpoint::ConsumeLastTriggered() ==
               (temporary_first ? permanent_id : temporary_id));
        assert(!CBreakpoint::DeleteById(temporary_id));
        assert(memory[0x1234] == 0xcc);
        CBreakpoint::DeactivateBreakpoints();
        assert(memory[0x1234] == 0x48);
        assert(CBreakpoint::DeleteById(permanent_id));
    }
    // Bulk retirement must unlink each freed object before deactivating peers.
    memory[0x1234] = 0x51;
    CBreakpoint::AddBreakpoint(0x120, 0x34, false);
    CBreakpoint::AddBreakpoint(0x120, 0x34, true);
    CBreakpoint::ActivateBreakpoints();
    CBreakpoint::DeleteAll();
    assert(memory[0x1234] == 0x51);
    // A guest-provided INT3 must not be replaced by invented instruction data.
    memory[0x1234] = 0xcc;
    auto *int3 = CBreakpoint::AddBreakpoint(0x120, 0x34, false);
    int3->Activate(true);
    CBreakpoint::DeleteAll();
    assert(memory[0x1234] == 0xcc && diagnostics == 2 && writes > 0);
}
'''
(work / 'patches.cpp').write_text(source)
subprocess.run(['g++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                str(work / 'patches.cpp'), '-o', str(work / 'patches')], check=True)
subprocess.run([str(work / 'patches')], check=True)
print('actual non-heavy breakpoint patch/retirement bodies PASS; bounded memory, not live CPU/session qualification')

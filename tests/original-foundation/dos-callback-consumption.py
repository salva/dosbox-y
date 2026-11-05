#!/usr/bin/env python3
"""Actual DOS native callback role checks with reduced callback/memory doubles.
Integrated fresh target success/disposal is tested separately.
"""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'dos-callback-consumption'
work.mkdir(exist_ok=True)
text = (root / 'src/dos/dos.cpp').read_text()
body = text.split('class DOS:public Module_base{', 1)[1].split('\tvoid DOS_Write_HMA_CPM_jmp', 1)[0]
source = r'''
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
using Bitu = unsigned;
using PhysPt = unsigned;
using RealPt = unsigned;
using CallBack_Handler = Bitu(*)();
constexpr unsigned CB_MAX = 128;
CallBack_Handler CallBack_Handlers[CB_MAX] = {};
uint8_t ram[0x110000] = {};
uint8_t phys_readb(unsigned address) { assert(address < sizeof(ram)); return ram[address]; }
PhysPt CALLBACK_PhysPointer(unsigned slot) { return 32 * slot; }
uint8_t mem_readb(unsigned address) { assert(address < sizeof(ram)); return ram[address]; }
unsigned next_slot = 1;
struct CALLBACK_HandlerObject {
    unsigned slot = 0;
    Bitu Install(CallBack_Handler handler, Bitu type, const char*) {
        slot = next_slot++; CallBack_Handlers[slot] = handler;
        for (unsigned n = 0; n < type; ++n) ram[32 * slot + n] = uint8_t(slot + n);
        return type;
    }
    unsigned Get_callback() { return slot; }
};
class DOS {
''' + body.replace('private:', 'public:', 1) + r'''
};
Bitu handler() { return 0; }
Bitu wrong() { return 1; }
int main() {
    DOS owner;
    assert(owner.NativeCallbacksIntact());
    owner.native_hma_cpm_body={0xea,1,2,3,4};
    for (unsigned n=0;n<5;++n) ram[0x1000c0+n]=owner.native_hma_cpm_body[n];
    assert(owner.NativeCallbacksIntact());
    for (unsigned n=0;n<5;++n) {
        ram[0x1000c0+n]^=1;assert(!owner.NativeCallbacksIntact());ram[0x1000c0+n]^=1;
    }
    for (unsigned role = 0; role < 9; ++role)
        owner.InstallCallback(role, role == 5 ? nullptr : handler, 5 + role, "role");
    assert(owner.NativeCallbacksIntact());
    for (unsigned role = 0; role < 9; ++role) {
        auto &native = owner.native_callback[role];
        auto slot = native.slot;
        auto original_handler = CallBack_Handlers[slot];
        CallBack_Handlers[slot] = wrong;
        assert(!owner.NativeCallbacksIntact());
        CallBack_Handlers[slot] = original_handler;
        owner.callback[role].slot = CB_MAX;
        assert(!owner.NativeCallbacksIntact());
        owner.callback[role].slot = 0;
        assert(!owner.NativeCallbacksIntact());
        owner.callback[role].slot = slot;
        ++native.slot; assert(!owner.NativeCallbacksIntact()); --native.slot;
        ++native.address; assert(!owner.NativeCallbacksIntact()); --native.address;
        for (unsigned n = 0; n < native.body.size(); ++n) {
            ram[native.address + n] ^= 1;
            assert(!owner.NativeCallbacksIntact());
            ram[native.address + n] ^= 1;
        }
        assert(owner.NativeCallbacksIntact());
    }
}
'''
(work / 'test.cpp').write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-fsanitize=undefined',
                str(work / 'test.cpp'), '-o', str(work / 'test')], check=True)
subprocess.run([str(work / 'test')], check=True)
print('Actual DOS callback role checks: handler/slot/location/span failures reject (reduced UBSan doubles)')

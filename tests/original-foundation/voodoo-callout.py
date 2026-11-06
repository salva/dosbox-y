#!/usr/bin/env python3
"""Actual Voodoo callout owner: presence, PCI enable, BAR, independent release."""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
src = (root/'src/hardware/voodoo.cpp').read_text()
a = src.index('void voodoo_lfb_cb_free(void) {')
b = src.index('\nclass VOODOO:', a)
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'voodoo-callout'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p = Path(d)
 (p/'probe.cpp').write_text('''#include <cassert>
#include <cstdint>
#include <cstddef>
using MEM_Callout_t=int;
constexpr int MEM_Callout_t_none=0, MEM_TYPE_PCI=1, MEMMASK_FULL=0;
constexpr unsigned VOODOO_PAGES=4096;
static bool voodoo_device_enabled=false, voodoo_pci_enabled=true;
static uint32_t voodoo_current_lfb=0xe0000000;
static MEM_Callout_t voodoo_lfb_cb=0;
static unsigned allocations=0, releases=0, puts=0;
struct MEM_CalloutObject {
 bool installed=false;unsigned page=0;
 void Uninstall(){installed=false;}
 void Install(unsigned p,unsigned,int){installed=true;page=p;}
} callout;
int voodoo_lfb_memio_cb=0;
int MEM_AllocateCallout(int){++allocations;return 7;}
void MEM_FreeCallout(int h){assert(h==7);++releases;callout.installed=false;}
MEM_CalloutObject* MEM_GetCallout(int h){assert(h==7);return &callout;}
void MEM_PutCallout(MEM_CalloutObject* p){assert(p==&callout);++puts;}
unsigned MEMMASK_Range(unsigned){return 0;}
unsigned MEMMASK_Combine(unsigned,unsigned){return 0;}
void E_Exit(const char*){assert(false);}
#define LOG_MSG(...) ((void)0)
'''+src[a:b]+'''
int main(){
 voodoo_lfb_cb_init();assert(!allocations&&!puts&&!voodoo_lfb_cb);
 voodoo_device_enabled=true;voodoo_lfb_cb_init();
 assert(allocations==1&&callout.installed&&callout.page==0xe0000);
 voodoo_pci_enabled=false;voodoo_lfb_cb_init();
 assert(allocations==1&&!callout.installed&&voodoo_lfb_cb==7);
 voodoo_pci_enabled=true;voodoo_current_lfb=0;voodoo_lfb_cb_init();
 assert(!callout.installed&&allocations==1);
 voodoo_current_lfb=0xd0000000;voodoo_lfb_cb_init();
 assert(callout.installed&&callout.page==0xd0000);
 voodoo_device_enabled=false;voodoo_lfb_cb_init();
 assert(releases==1&&!voodoo_lfb_cb&&!callout.installed);
 voodoo_lfb_cb_free();assert(releases==1);
 voodoo_device_enabled=true;voodoo_lfb_cb_init();
 assert(allocations==2&&callout.installed);
 voodoo_lfb_cb_free();assert(releases==2&&!voodoo_lfb_cb);
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
 subprocess.run([str(p/'probe')],check=True)
print('Voodoo actual callout owner presence/PCI/BAR/release ASan/UBSan PASS')

#!/usr/bin/env python3
"""Actual DMA callback reverse lookup rejects every invalid byte ID."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dma-callback';w.mkdir(exist_ok=True)
t=(root/'src/hardware/dma.cpp').read_text()
a=t.index('Bitu POD_State_Index_DMA_Callback(');b=t.index('void DmaChannel::SaveState',a)
s='''
#include <cstdint>
#include <cstddef>
#include <cassert>
using Bitu=uintptr_t;
int entries[5];
const void *dma_state_callback_table[]={nullptr,&entries[0],&entries[1],&entries[2],&entries[3],&entries[4]};
[[noreturn]] void E_Exit(const char*) {throw 7;}
'''+t[a:b]+'''
int main(){for(unsigned id=0;id<256;++id){bool fatal=false;Bitu value=0;
 try{value=POD_State_Index_DMA_Callback(id);}catch(int){fatal=true;}
 assert(fatal==(id>=6));if(id<6)assert(value==reinterpret_cast<Bitu>(dma_state_callback_table[id]));
}}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DMA callback all byte IDs ASan/UBSan PASS; not channel/resource qualification')

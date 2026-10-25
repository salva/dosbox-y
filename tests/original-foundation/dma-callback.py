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
# Also compile actual channel Save/Load with the native field declaration.
h=(root/'include/dma.h').read_text()
a=h.index('\t\tuint32_t pagebase;');b=h.index('\n\t\t// additional',a)
fields=h[a:b]
a=t.index('void DmaChannel::SaveState(');b=t.index('void DmaController::SaveState',a)
methods=t[a:b]
a=t.index('uint8_t POD_State_Find_DMA_Callback(');b=t.index('Bitu POD_State_Index_DMA_Callback',a)
find=t[a:b]
s=s[:s.index('int main()')]+'''\n#include <sstream>
#include <string>
enum DMAEvent {Unused}; enum DMAActor {None};
struct DmaChannel;using DMA_CallBack=void(*)(DmaChannel*,DMAEvent);
struct DmaChannel {\n'''+fields+'''
 void SaveState(std::ostream&);void LoadState(std::istream&);
};
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
'''+find+methods+'''
int main(){
 for(unsigned id=0;id<256;++id){bool fatal=false;Bitu value=0;
 try{value=POD_State_Index_DMA_Callback(id);}catch(int){fatal=true;}
 assert(fatal==(id>=6));if(id<6)assert(value==reinterpret_cast<Bitu>(dma_state_callback_table[id]));}
 DmaChannel original={};original.channum=3;original.DMA16=0;
 auto capture=[](DmaChannel c){std::ostringstream out;c.SaveState(out);return out.str();};
 const auto bytes=capture(original);
 for(size_t n=0;n<bytes.size();++n){auto c=original;std::istringstream in(bytes.substr(0,n));bool fatal=false;
 try{c.LoadState(in);}catch(int){fatal=true;}assert(fatal);}
 auto c=original;std::istringstream good(bytes);c.LoadState(good);assert(good);
 for(unsigned mode=0;mode<2;++mode){auto bad=original;if(mode)bad.DMA16=1;else bad.channum=7;
 auto dest=original;std::istringstream in(capture(bad));bool fatal=false;
 try{dest.LoadState(in);}catch(int){fatal=true;}assert(fatal);}
}
'''
(w/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual DMA callback all byte IDs ASan/UBSan PASS; not channel/resource qualification')

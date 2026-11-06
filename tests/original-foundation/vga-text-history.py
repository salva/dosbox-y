#!/usr/bin/env python3
"""Compile the actual owner helper and traversal fields, cross-packing recurrence."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/vga_draw.cpp').read_text()
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-text-history';w.mkdir(exist_ok=True)
a=s.index('    WRITE_POD(&text_history.valid,');b=s.index('\n}',a)
c=s.index('    READ_POD(&text_history.valid,');d=s.index('\n',s.index('    if (!stream || !TextHistoryValid())',c))
(w/'probe.cpp').write_text('''#include <cstdint>
#include <cstring>
#include <cassert>
#include <sstream>
#include <stdexcept>
unsigned GFX_Rshift=16,GFX_Gshift=8,GFX_Bshift=0;
struct Draw{unsigned width=640,bpp=32;}draw;struct VGA{Draw&draw;}vga{draw};
uint8_t storage[33792]={};uint8_t*TempLine=storage;unsigned TempLineSize=sizeof(storage);
void E_Exit(const char*){throw std::runtime_error("bad history");}
'''+(root/'src/hardware/vga_text_history.h').read_text()+'''
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
void save(std::ostream&stream){
'''+s[a:b]+'''}
void load(std::istream&stream){
'''+s[c:d]+'''}
int main(){
 TextHistoryUniform(0x12345678,4);TextHistoryBegin();assert(text_history.nibble==8);
 // A source packed pixel feeds the tail byte on the source blue channel.
 unsigned at=16+(draw.width&~1u);uint32_t pixel=0x0011223b;memcpy(storage+at,&pixel,4);
 TextHistoryObserve();assert(text_history.nibble==11);
 std::ostringstream out;save(out);assert(out.str().size()==16);
 auto source=text_history;TextHistoryForget();GFX_Rshift=0;GFX_Bshift=16;
 std::istringstream in(out.str());load(in);assert(in && text_history.nibble==11);
 TextHistoryBegin();assert((storage[at]&15)==11);
 // New destination bytes are reversed; feedback remains source blue arithmetic.
 pixel=0x003b2211;memcpy(storage+at,&pixel,4);TextHistoryObserve();assert(text_history.nibble==11);
 TextHistoryXor(storage+64,true,false,0);assert(text_history.nibble==4);
 // Preserve original overlay blue ADD (not shift) under source packing.
 const uint8_t rgb[3]={0x31,0x42,0x5b};TextHistoryOverlay(rgb,at);
 assert(text_history.nibble==11);
 TextHistoryOverlay(rgb,at+4);assert(text_history.nibble==11);
 draw.width=642;text_history=source;at=16+(draw.width&~1u);
 text_history.width=draw.width;text_history.nibble=9;
 TextHistoryXor(storage+64,false,true,1); // byte2 does not match polled byte1
 assert(text_history.nibble==9);
 auto bytes=out.str();for(size_t n=0;n<bytes.size();++n){std::istringstream shortin(bytes.substr(0,n));load(shortin);assert(!shortin);}
 bytes[0]=2;std::istringstream bad(bytes);load(bad);assert(!bad);
 text_history=TextProducerHistory{};TextHistoryUniform(0x1234,2);draw.width=641;TextHistoryBegin();assert(text_history.nibble==4);
 TextHistoryForget();assert(!text_history.valid && !text_history.uniform);
}
''')
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(w/'probe.cpp'),'-o',str(w/'probe')],check=True)
subprocess.run([str(w/'probe')],check=True)
print('Original owner actual helper + codec cross-channel/uniform/XOR/truncation ASan/UBSan PASS')

#!/usr/bin/env python3
"""Actual live text consuming-boundary guards; no mock replacement formula."""
import os,subprocess
from pathlib import Path
s=(Path(__file__).resolve().parents[2]/'src/hardware/vga_draw.cpp').read_text()
a=s.index('    // The live producer writes complete glyph blocks')
b=s.index('    if (vga.crtc.maximum_scan_line',a)
c=s.index('    // Byte glyph output and expanded packed32 output')
d=s.index('\n\t/* Render it like EGA',c)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-text-bounds';w.mkdir(exist_ok=True)
p=w/'probe.cpp'
p.write_text('''#include <cstdint>
#include <cassert>
#include <stdexcept>
constexpr unsigned MCH_RAW_SNAPSHOT=99;
struct Draw{unsigned blocks=80,panning=0,width=640;bool char9dot=false;}draw;
struct VGA{Draw&draw;}vga{draw};unsigned TempLineSize=33792;
void E_Exit(const char*){throw std::runtime_error("extent");}
template<unsigned card,typename templine_type_t>void glyph(uint8_t*dst){
'''+s[a:b]+'''}
void smzx(uint8_t*dst){
'''+s[c:d]+'''}
template<class F>void denied(F f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){uint8_t byte=0;glyph<1,uint32_t>(&byte);smzx(&byte);
 TempLineSize=(640+16)*4;glyph<1,uint32_t>(&byte);draw.panning=1;
 denied([&]{glyph<1,uint32_t>(&byte);});draw.panning=17;TempLineSize=33792;
 denied([&]{glyph<1,uint32_t>(&byte);});draw.panning=0;draw.blocks=~0u;
 denied([&]{glyph<1,uint32_t>(&byte);});draw.blocks=80;draw.char9dot=true;
 TempLineSize=(720+16)*4;glyph<1,uint32_t>(&byte);--TempLineSize;
 denied([&]{glyph<1,uint32_t>(&byte);});glyph<MCH_RAW_SNAPSHOT,uint8_t>(nullptr);
 draw.width=1;smzx(&byte);draw.width=641;TempLineSize=16+641*4;
 smzx(&byte);--TempLineSize;denied([&]{smzx(&byte);});
 denied([&]{smzx(nullptr);});}
''')
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p),'-o',str(w/'probe')],check=True)
subprocess.run([str(w/'probe')],check=True)
print('Actual live text glyph/panning/8/9dot/SMZX expanded boundary guards ASan/UBSan PASS')

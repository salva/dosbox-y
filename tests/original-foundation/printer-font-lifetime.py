#!/usr/bin/env python3
"""Actual printer TTF face factory + native FreeType lifetime (not print parity)."""
import os, pathlib, shlex, subprocess
root=pathlib.Path(__file__).resolve().parents[2]
s=(root/'src/output/output_ttf.cpp').read_text()
body=s[s.index('FT_Face GetTTFFace('):s.index('\nvoid resetFontSize()',s.index('FT_Face GetTTFFace('))]
work=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'printer-font-lifetime'
work.mkdir(exist_ok=True)
# Project-owned deterministic embedded face; no host font dependency.
text=(root/'include/DOSBoxTTF.h').read_text()
start=text.index('unsigned char DOSBoxTTFbi[48868]')
end=text.index('};',start)+2
array=text[start:end]
prefix='''
#include <cassert>
#include <memory>
#include <vector>
#include <limits>
#include <ft2build.h>
#include FT_FREETYPE_H
struct {bool inUse=true; int pointsize=12;} ttf;
void *ttfFont=nullptr; unsigned long ttfSize=0;
'''
main='''
int main(){
 FT_Library lib=nullptr;assert(!FT_Init_FreeType(&lib));
 std::shared_ptr<std::vector<unsigned char>> backing;
 assert(!GetTTFFace(lib,backing));assert(!backing);
 for(int i=0;i<24;++i){
  std::vector<unsigned char> display(DOSBoxTTFbi,DOSBoxTTFbi+sizeof DOSBoxTTFbi);
  ttfFont=display.data();ttfSize=display.size();ttf.inUse=true;
  FT_Face face=GetTTFFace(lib,backing);assert(face && backing);
  // Display reload/destruction cannot invalidate the independently owned face.
  display.assign(display.size(),0);ttfFont=nullptr;ttfSize=0;
  assert(!FT_Load_Char(face,'A',FT_LOAD_RENDER));assert(face->glyph->bitmap.rows);
  FT_Done_Face(face);backing.reset();
 }
 ttfFont=DOSBoxTTFbi;ttfSize=sizeof DOSBoxTTFbi;ttf.inUse=false;
 assert(!GetTTFFace(lib,backing));assert(!backing);
 ttf.inUse=true;unsigned char invalid[16]={};ttfFont=invalid;ttfSize=sizeof invalid;
 assert(!GetTTFFace(lib,backing));assert(!backing);
 FT_Done_FreeType(lib);
}
'''
(work/'test.cpp').write_text(prefix+array+'\n'+body+main)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','freetype2'],text=True))
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'test.cpp'),'-o',str(work/'test'),*flags],check=True)
subprocess.run([str(work/'test')],check=True)
print('Production printer face factory: 24 display-reload/render/dispose cycles, absent/disabled/invalid faces ASan/UBSan PASS')

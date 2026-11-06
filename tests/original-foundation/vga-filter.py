#!/usr/bin/env python3
"""Actual owner filter/latch bytes and next-scanline filter update witness."""
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/'src/hardware/vga_draw.cpp').read_text()
# Extract the actual save/load statements and the actual future-address filter.
fields=('hretrace_fx_avg','vga_display_start_hretrace','is_vga_rendering_on_demand')
save='\n'.join(next(l for l in src.splitlines() if 'WRITE_POD( &'+f+',' in l) for f in fields)
load='\n'.join(next(l for l in src.splitlines() if 'READ_POD( &'+f+',' in l) for f in fields)
a=src.index('        float a = 1.0 / (hretrace_fx_avg_weight + 1);')
b=src.index('        int x = (int)floor(hretrace_fx_avg + 0.5);',a)
update=src[a:b]+src[b:src.index('\n',b)]+'\nreturn x;'
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'vga-filter'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'t.cpp').write_text('''#include <cassert>
#include <sstream>
#include <cmath>
struct {struct {int start_horizontal_retrace;} crtc;} vga;
float hretrace_fx_avg=0,hretrace_fx_avg_weight=3;
int vga_display_start_hretrace=12;
bool is_vga_rendering_on_demand=false;
#define WRITE_POD(p,x) stream.write(reinterpret_cast<const char*>(p),sizeof(x))
#define READ_POD(p,x) stream.read(reinterpret_cast<char*>(p),sizeof(x))
void save(std::ostream &stream){'''+save+'''}
void load(std::istream &stream){'''+load+'''}
int next(){'''+update+'''}
int main(){
 vga.crtc.start_horizontal_retrace=10;
 for(float history:{-12.f,0.f,17.5f})for(bool demand:{false,true}){
  hretrace_fx_avg=history;is_vga_rendering_on_demand=demand;
  std::stringstream out;save(out);auto bytes=out.str();
  int expected=next();float expected_history=hretrace_fx_avg;
  hretrace_fx_avg=99;vga_display_start_hretrace=99;is_vga_rendering_on_demand=!demand;
  std::stringstream in(bytes);load(in);assert(in);
  assert(hretrace_fx_avg==history && is_vga_rendering_on_demand==demand);
  assert(next()==expected && hretrace_fx_avg==expected_history);
  for(unsigned n=0;n<bytes.size();++n){std::stringstream short_in(bytes.substr(0,n));load(short_in);assert(!short_in);}
 }
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Actual VGA retrace filter/demand latch owner bytes + next addressing update/truncation ASan/UBSan PASS')

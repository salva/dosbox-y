#!/usr/bin/env python3
"""Extract actual original Int10 mode traversal; poison and relocate identities."""
import os,re,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'src/ints/int10_modes.cpp').read_text()
a=s.index('// Original Int10 traversal:')
codec=s[a:]
names=re.findall(r'MODE_SPAN\((ModeList_\w+)\)',codec)
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'int10-modes';work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 declarations='\n'.join('VideoModeBlock '+n+'[3]{};' for n in names)
 (p/'t.cpp').write_text('''#include <sstream>
#include <cstdint>
#include <cassert>
struct VideoModeBlock {uint16_t mode;int type;unsigned swidth,sheight;};
VideoModeBlock Hercules_Mode{},PC98_Mode{},*CurMode=nullptr;
#define WRITE_POD(p,x) stream.write(reinterpret_cast<const char*>(p),sizeof(x))
#define READ_POD(p,x) stream.read(reinterpret_cast<char*>(p),sizeof(x))
'''+declarations+'\n'+codec+'''
int main(){
 for(const auto &span:mode_spans)for(unsigned i=0;i<span.count;++i){
  span.data[i].mode=uint16_t(31+i);span.data[i].type=int(i)-1;
  span.data[i].swidth=640+i;span.data[i].sheight=200+i;
 }
 // Last/sentinel cells and all table aliases, not only normal mode entries.
 for(const auto &span:mode_spans)for(unsigned i=0;i<span.count;++i){
  CurMode=span.data+i;std::stringstream o;POD_Save_Int10_Modes(o);auto bytes=o.str();
  auto expected=*CurMode;CurMode=nullptr;span.data[i]={};
  std::stringstream in(bytes);POD_Load_Int10_Modes(in);assert(in);
  assert(CurMode==span.data+i && CurMode->mode==expected.mode && CurMode->swidth==expected.swidth);
 }
 VideoModeBlock detached{799,12,123,456};CurMode=&detached;
 std::stringstream out;POD_Save_Int10_Modes(out);auto bytes=out.str();
 detached={};CurMode=nullptr;std::stringstream in(bytes);POD_Load_Int10_Modes(in);
 assert(in && CurMode!=&detached && CurMode==&detached_checkpoint_mode && CurMode->sheight==456);
 for(unsigned n=0;n<bytes.size();++n){std::stringstream cut(bytes.substr(0,n));POD_Load_Int10_Modes(cut);assert(!cut);}
 CurMode=nullptr;std::stringstream null;POD_Save_Int10_Modes(null);CurMode=&detached;
 POD_Load_Int10_Modes(null);assert(null && !CurMode);
}
''')
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('Actual Int10 mode owner traversal all table aliases/sentinels/detached/null/truncations ASan/UBSan PASS')

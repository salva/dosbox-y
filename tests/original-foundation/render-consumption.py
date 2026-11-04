#!/usr/bin/env python3
"""Actual render codec with stub backend; no SDL/backend-restore proof."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'render-consumption'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/gui/render.cpp').read_text()
codec=t[t.index('namespace\n{\nclass SerializeRender'):]
source=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
struct {struct {uintptr_t width=640,height=480,bpp=8;float fps=60;double scrn_ratio=4.0/3;}src;
 struct {uint8_t rgb[256][4]={},modified[256]={};uintptr_t first=256,last=0;bool changed=false;}pal;
 struct {bool clearCache=false;void *native=(void*)0x1234;}scale;}render;
unsigned resets=0;
void RENDER_SetSize(uintptr_t,uintptr_t,uintptr_t,float,double){++resets;}
'''+codec+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 const auto bytes=save();
 assert(bytes.size()==3*sizeof(uintptr_t)+sizeof(float)+sizeof(double)+1024);
 for(size_t i=0;i<bytes.size();++i){assert(!load(bytes.substr(0,i)));assert(!resets);}
 assert(load(bytes));assert(resets==1 && render.scale.native==(void*)0x1234);
 assert(render.pal.first==0 && render.pal.last==255 && render.scale.clearCache);
 render.src.width=0;auto bad=save();assert(!load(bad) && resets==1);assert(load(bytes));
 render.src.bpp=24;bad=save();assert(!load(bad) && resets==2);assert(load(bytes));
 render.src.fps=NAN;bad=save();assert(!load(bad) && resets==3);assert(load(bytes));
 render.src.scrn_ratio=INFINITY;bad=save();assert(!load(bad) && resets==4);
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Render codec: pointer-free wire, all truncations, geometry/fps/ratio refusal before backend passed')

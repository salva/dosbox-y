#!/usr/bin/env python3
"""Actual Render owner codec, destination-local stub backend; not SDL proof."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'render-consumption'
work.mkdir(exist_ok=True)
h=(root/'include/dosbox.h').read_text()
pod=h[h.index('class SerializeGlobalPOD :'):h.index('//---------------- inline implementation')]
t=(root/'src/gui/render.cpp').read_text()
codec=t[t.index('namespace\n{\nclass SerializeRender'):]
a=t.index('static void RENDER_StartLineHandler(')
b=t.index('\nstatic void RENDER_FinishLineHandler',a)
handler=t[a:b]
r=(root/'include/render.h').read_text()
structures=r[r.index('typedef struct {'):r.index('#if defined(USE_TTF)')]
structures=structures.replace('#if C_OPENGL','#if 0')
source=r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
using Bitu=uintptr_t;
using ScalerLineHandler_t=void(*)(const void*);
using ScalerComplexHandler_t=void(*)(const void*);
using scalerMode_t=int;using scalerOperation_t=int;
constexpr int scalerMode32=32;
#define RENDER_SKIP_CACHE 16
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+structures+r'''
template<class T> void writePOD(std::ostream &s,const T &v){s.write(reinterpret_cast<const char*>(&v),sizeof(v));}
template<class T> void readPOD(std::istream &s,T &v){s.read(reinterpret_cast<char*>(&v),sizeof(v));}
Render_t render{};
struct {struct {bool must_complete_frame=false;}draw;}vga;
bool render_restoring_resources=false,useTraditionalRenderCache=false;
uint8_t local_cache[8192]{},aspects[32]{},canvas[512]{};
uint8_t *scalerSourceCacheBuffer=local_cache,*Scaler_Aspect=aspects;
unsigned scalerSourceCacheBufferSize=sizeof(local_cache);
uint16_t lines[32]{};uint16_t *Scaler_ChangedLines=lines;
Bitu Scaler_ChangedLineIndex=0;
void RENDER_EmptyLineHandler(const void*){}
static void RENDER_StartLineHandler(const void*);
bool hit=true;unsigned acquisitions=0,scales=0;
bool RENDER_DrawLine_scanline_cacheHit(const void*){return hit;}
bool GFX_StartUpdate(uint8_t *&p,Bitu &pitch){++acquisitions;p=canvas;pitch=16;return true;}
void RENDER_FinishLineHandler(const void*){}
void RENDER_ScalerLineHandler(const void*){++scales;}
void RENDER_ClearCacheHandler(const void*){}
ScalerLineHandler_t RENDER_DrawLine=RENDER_StartLineHandler;
unsigned resets=0;
void RENDER_SetSize(Bitu,Bitu,Bitu,float,double){++resets;render.scale.cacheRead=nullptr;}
uint32_t GFX_GetRGB(uint8_t r,uint8_t g,uint8_t b){return (r<<16)|(g<<8)|b;}
bool GFX_RenderHistory(std::ostream *o,std::istream *i,uint8_t *&cursor,Bitu &pitch){
 uint8_t acquired=cursor?1:0;Bitu row=cursor?Bitu(cursor-canvas)/16:0;
 if(o){writePOD(*o,acquired);writePOD(*o,row);}else{readPOD(*i,acquired);readPOD(*i,row);if(!*i||acquired>1||row>32)return false;cursor=acquired?canvas+row*16:nullptr;pitch=acquired?16:0;}
 return true;
}
'''+handler+codec+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 render.src.width=8;render.src.height=8;render.src.bpp=32;render.src.fps=60;render.src.scrn_ratio=4.0/3;
 render.scale.outMode=32;render.scale.cachePitch=32;render.scale.cacheRead=local_cache+96;
 // Simple scalers advance cacheRead/outLine without advancing inLine.
 render.scale.inLine=0;render.scale.outLine=3;render.updating=true;
 render.scale.outWrite=canvas+80;render.scale.outPitch=16;
 aspects[0]=2;aspects[1]=3;lines[0]=5;local_cache[97]=71;
 const auto bytes=save();
 render.scale.cacheRead=local_cache;render.scale.outWrite=nullptr;render.scale.outLine=0;
 assert(load(bytes));assert(render.scale.cacheRead==local_cache+96);
 assert(render.scale.inLine==0 && render.scale.outLine==3 && render.scale.outWrite==canvas+80);
 assert(RENDER_DrawLine==RENDER_StartLineHandler && aspects[1]==3 && lines[0]==5 && local_cache[97]==71);
 // Actual acquired-output cache-hit -> cache-hit -> miss after owner import.
 render.scale.inLine=render.scale.outLine=0;render.scale.cacheRead=local_cache;
 render.scale.outWrite=canvas+16;render.scale.outPitch=16;Scaler_ChangedLineIndex=0;
 lines[0]=0;aspects[0]=2;aspects[1]=3;const auto witness=save();
 render.scale.outWrite=nullptr;assert(load(witness));hit=true;
 RENDER_DrawLine(nullptr);RENDER_DrawLine(nullptr);
 assert(render.scale.outWrite==canvas+96 && lines[0]==5);
 hit=false;RENDER_DrawLine(nullptr);
 assert(!acquisitions && scales==1 && render.scale.outWrite==canvas+96);
 for(size_t i=0;i<bytes.size();++i)assert(!load(bytes.substr(0,i)));
 assert(load(bytes));
 render.src.width=0;auto bad=save();assert(!load(bad));assert(load(bytes));
 render.src.bpp=24;bad=save();assert(!load(bad));assert(load(bytes));
 render.src.fps=NAN;bad=save();assert(!load(bad));assert(load(bytes));
 render.src.scrn_ratio=INFINITY;bad=save();assert(!load(bad));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-fsanitize=address,undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('Actual Render codec cache offset/phase/acquired cursor/aspect/history roundtrip, all truncations, invalid geometry PASS (stub backend)')

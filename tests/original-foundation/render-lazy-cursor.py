#!/usr/bin/env python3
"""Exercise the actual lazy handler with acquired and unacquired output."""
import os, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[2]
s = (root / 'src/gui/render.cpp').read_text()
a = s.index('static void RENDER_StartLineHandler(')
b = s.index('\nstatic void RENDER_FinishLineHandler', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'render-lazy-cursor'
w.mkdir(exist_ok=True)
(w / 'probe.cpp').write_text(r'''
#include <cassert>
#include <cstdint>
using Bitu=unsigned;
using ScalerLineHandler_t=void(*)(const void*);
struct {bool disablerender=false;struct {uint8_t *cacheRead,*outWrite; Bitu cachePitch=8,outPitch=16,inLine=0,outLine=0;} scale;} render;
uint8_t cache[128]{},output[512]{};uint8_t Scaler_Aspect[16]{2,3,1};
uint16_t Scaler_ChangedLines[16]{};bool hit=true,accept=true;unsigned acquisitions=0,scales=0;
ScalerLineHandler_t RENDER_DrawLine;
bool RENDER_DrawLine_scanline_cacheHit(const void*){return hit;}
void RENDER_EmptyLineHandler(const void*){}
void RENDER_ScalerLineHandler(const void*){++scales;}
bool GFX_StartUpdate(uint8_t *&p,Bitu &pitch){++acquisitions;if(!accept)return false;p=output;pitch=16;return true;}
''' + s[a:b] + r'''
void reset(bool acquired){render.scale.cacheRead=cache;render.scale.outWrite=acquired?output+16:nullptr;render.scale.inLine=render.scale.outLine=0;Scaler_ChangedLines[0]=0;acquisitions=scales=0;hit=true;accept=true;}
int main(){
 reset(true);RENDER_StartLineHandler(nullptr);RENDER_StartLineHandler(nullptr);
 assert(render.scale.outWrite==output+96 && render.scale.cacheRead==cache+16);
 assert(render.scale.inLine==2 && render.scale.outLine==2 && Scaler_ChangedLines[0]==5);
 hit=false;RENDER_StartLineHandler(nullptr);
 assert(!acquisitions && scales==1 && render.scale.outWrite==output+96);
 reset(false);RENDER_StartLineHandler(nullptr);RENDER_StartLineHandler(nullptr);
 assert(!render.scale.outWrite);hit=false;RENDER_StartLineHandler(nullptr);
 assert(acquisitions==1 && scales==1 && render.scale.outWrite==output+80);
 reset(false);hit=false;accept=false;RENDER_StartLineHandler(nullptr);
 assert(acquisitions==1 && !scales && RENDER_DrawLine==RENDER_EmptyLineHandler);
 reset(true);render.disablerender=true;RENDER_StartLineHandler(nullptr);
 assert(render.scale.outWrite==output+16 && !render.scale.inLine);
}
''')
subprocess.run(['c++', '-std=c++14', '-fsanitize=address,undefined', str(w/'probe.cpp'), '-o', str(w/'probe')], check=True)
subprocess.run([str(w/'probe')], check=True)
print('Actual lazy handler acquired hit-hit-miss/aspect repeat/unacquired/refusal/disabled PASS')

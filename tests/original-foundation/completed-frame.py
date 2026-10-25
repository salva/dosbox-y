#!/usr/bin/env python3
"""Completed frame clock/observer seam, not PNG delivery qualification."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'completed-frame-regression'
work.mkdir(exist_ok=True)
t=(root/'src/gui/render.cpp').read_text()
a=t.index('static CaptureFrame::Clock completed_frame_clock;')
seam=t[a:t.index('void RENDER_EndUpdate',a)]
# Extract the consumer construction as well as the clock, so flag/pointer joins
# are exercised without introducing a production helper solely for this test.
v=t.index('    // Static pixels still receive a new identity;')
view=t[v:t.index('    RENDER_DrawLine = RENDER_EmptyLineHandler;',v)]
s='#include "capture_frame.h"\n#include <cassert>\n'+seam+r'''
struct { struct {bool dblw=false,dblh=false;unsigned width=320,height=200,bpp=8;} src;
 struct {size_t cachePitch=320;} scale;
 struct {uint8_t rgb[256*3]={};} pal;
 bool updating=true,active=true,disablerender=false;
} render;
uint8_t cache[8]={};void *scalerSourceCacheBuffer=cache;
unsigned Scaler_ChangedLineIndex=0;uint32_t PIC_Ticks=42;
enum {CAPTURE_FLAG_DBLW=1,CAPTURE_FLAG_DBLH=2,CAPTURE_FLAG_NOCHANGE=4};
void end(bool abort){
'''+view+r'''
}
unsigned joined=0;
void joined_observer(const CaptureFrame::View &v,void *){
 ++joined;assert(v.sequence==joined && v.guest_tick==42);
 assert(v.width==320 && v.height==200 && v.bpp==8 && v.pitch==320);
 assert(v.pixels==cache && v.palette==render.pal.rgb);
 assert(v.flags==(joined==1 ? (CAPTURE_FLAG_DBLW|CAPTURE_FLAG_NOCHANGE):CAPTURE_FLAG_DBLH));
}
unsigned calls=0;
void observe(const CaptureFrame::View &v,void *p){assert(p==&calls);++calls;assert(v.sequence==calls);}
int main(){
 using namespace CaptureFrame;
 View v={};Clock c;
 assert(RENDER_SetCompletedFrameObserver(observe,&calls));
 assert(!RENDER_SetCompletedFrameObserver(observe,&calls));
 for(unsigned flags=0;flags<16;++flags){
   bool u=flags&1,a=flags&2,d=flags&4,b=flags&8;
   bool expected=u&&a&&!d&&!b;
   assert(c.end(u,a,d,b,v,completed_frame_observer,completed_frame_context)==expected);
 }
 assert(calls==1 && c.sequence()==1);
 assert(c.end(true,true,false,false,v,completed_frame_observer,completed_frame_context));
 assert(calls==2); // unchanged pixels still complete
 assert(RENDER_SetCompletedFrameObserver(nullptr,nullptr));
 assert(completed_frame_context==nullptr);
 assert(RENDER_SetCompletedFrameObserver(observe,&calls));
 assert(RENDER_CompletedFrameSequence()==0 && !RENDER_CompletedFrameExhausted());
 Clock last(UINT64_MAX-1);
 assert(last.end(true,true,false,false,v,nullptr,nullptr));
 assert(last.sequence()==UINT64_MAX && !last.exhausted());
 assert(!last.end(true,true,false,false,v,nullptr,nullptr));assert(last.exhausted());
 assert(!last.end(true,true,false,false,v,nullptr,nullptr));
 assert(RENDER_SetCompletedFrameObserver(nullptr,nullptr));
 assert(RENDER_SetCompletedFrameObserver(joined_observer,nullptr));
 render.src.dblw=true;end(false);assert(joined==1);
 end(true);assert(joined==1);
 render.active=false;end(false);assert(joined==1);render.active=true;
 render.src.dblw=false;render.src.dblh=true;Scaler_ChangedLineIndex=1;
 end(false);assert(joined==2);
}
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('completed frame clock/observer/view ASan/UBSan PASS (not PNG/runtime lifetime)')

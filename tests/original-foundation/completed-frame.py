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
s='#include "capture_frame.h"\n#include <cassert>\n'+seam+r'''
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
}
'''
(work/'test.cpp').write_text(s)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root/'include'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('completed frame clock/observer ASan/UBSan PASS (not PNG/runtime lifetime)')

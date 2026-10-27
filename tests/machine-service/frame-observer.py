#!/usr/bin/env python3
"""Compile actual renderer observer registration/retirement, without renderer IO."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
area = Path(tempfile.mkdtemp(prefix='frame-observer-', dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
source = (root/'src/gui/render.cpp').read_text()
start = source.index('static CaptureFrame::Observer completed_frame_observer')
end = source.index('uint64_t RENDER_CompletedFrameSequence()', start)
preamble = '#include <cassert>\nnamespace CaptureFrame { using Observer=void (*)(void *); }\n'
main = r'''
static void first(void *) {}
static void second(void *) {}
int main() {
 int a=0,b=0;
 assert(!RENDER_RemoveCompletedFrameObserver(nullptr,nullptr));
 assert(RENDER_SetCompletedFrameObserver(first,&a));
 assert(!RENDER_SetCompletedFrameObserver(second,&b));
 assert(!RENDER_RemoveCompletedFrameObserver(first,&b));
 assert(!RENDER_RemoveCompletedFrameObserver(second,&a));
 assert(completed_frame_observer==first && completed_frame_context==&a);
 assert(RENDER_RemoveCompletedFrameObserver(first,&a));
 assert(!completed_frame_observer && !completed_frame_context);
 assert(RENDER_SetCompletedFrameObserver(second,&b));
 assert(!RENDER_RemoveCompletedFrameObserver(first,&a));
 assert(completed_frame_observer==second && completed_frame_context==&b);
 assert(RENDER_RemoveCompletedFrameObserver(second,&b));
}
'''
(area/'test.cpp').write_text(preamble+source[start:end]+main)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror',str(area/'test.cpp'),'-o',str(area/'test')],check=True)
subprocess.run([str(area/'test')],check=True)
print('actual frame observer identity retirement PASS (not production integration)')

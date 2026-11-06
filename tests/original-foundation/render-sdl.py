#!/usr/bin/env python3
"""Production Render+SDL history and lazy handler, real dummy SDL fresh processes.
Extraction shares the production owner bodies, not a production test hook.
This is not a complete emulator scan producer or every SDL backend test.
"""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'render-sdl'
work.mkdir(exist_ok=True)
# Reuse only the fixture declaration scaffolding, not its generated codec test.
fixture=(root/'tests/original-foundation/render-consumption.py').read_text()
fixture=fixture[:fixture.index("path=work/'consumer.cpp'")]
ns={'__file__':str(root/'tests/original-foundation/render-consumption.py')}
exec(compile(fixture,'render-consumption-scaffolding','exec'),ns)
source=ns['source']
a=source.index('bool GFX_RenderHistory(')
b=source.index('static void RENDER_StartLineHandler(',a)
sdl=(root/'src/gui/sdlmain.cpp').read_text()
a2=sdl.index('bool GFX_RenderHistory(')
b2=sdl.index('\nbool GFX_StartUpdate(',a2)
source=source[:a]+'''\n#define C_SDL2 1
#include <SDL.h>
#include <fstream>
constexpr int SCREEN_SURFACE=1;
struct {bool active=true,updating=false;struct{int type=SCREEN_SURFACE;}desktop;
SDL_Surface *surface=nullptr;struct{SDL_Surface *surface=nullptr;}blit;
SDL_Rect clip{};SDL_Window *window=nullptr;}sdl;
'''+sdl[a2:b2]+source[b:]
source=source[:source.index('int main(){')]+r'''
int main(int argc,char **argv){
 assert(argc==3); assert(SDL_Init(SDL_INIT_VIDEO)==0);
 sdl.window=SDL_CreateWindow("history",0,0,8,8,SDL_WINDOW_HIDDEN);
 assert(sdl.window);sdl.surface=SDL_GetWindowSurface(sdl.window);assert(sdl.surface);
 sdl.clip={0,0,8,8};
 auto *base=static_cast<uint8_t*>(sdl.surface->pixels);
 auto pitch=sdl.surface->pitch;auto bytes=sdl.surface->format->BytesPerPixel;
 assert(bytes==4);
 render.src.width=8;render.src.height=8;render.src.bpp=32;
 render.src.fps=60;render.src.scrn_ratio=4.0/3;
 render.scale.outMode=32;render.scale.cachePitch=32;
 render.scale.cacheRead=local_cache;render.updating=true;
 render.scale.outWrite=base+pitch;render.scale.outPitch=pitch;
 aspects[0]=2;aspects[1]=3;
 if(std::string(argv[1])=="save"){
  for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
   uint32_t p=SDL_MapRGB(sdl.surface->format,17+x,25+y,55);
   memcpy(base+y*pitch+x*bytes,&p,bytes);
  }
  const auto b=save();std::ofstream f(argv[2],std::ios::binary);f.write(b.data(),b.size());assert(f);
 }else{
  std::ifstream f(argv[2],std::ios::binary);std::string b((std::istreambuf_iterator<char>(f)),{});
  render.scale.outWrite=nullptr;assert(load(b));
  assert(render.scale.outWrite==base+pitch && render.scale.outPitch==Bitu(pitch));
  assert(sdl.updating);
  for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){
   uint32_t p=0;memcpy(&p,base+y*pitch+x*bytes,bytes);uint8_t r,g,blue;
   SDL_GetRGB(p,sdl.surface->format,&r,&g,&blue);assert(r==17+x && g==25+y && blue==55);
  }
  hit=true;RENDER_DrawLine(nullptr);RENDER_DrawLine(nullptr);
  assert(render.scale.outWrite==base+6*pitch && lines[0]==5);
  hit=false;RENDER_DrawLine(nullptr);
  assert(acquisitions==0 && scales==1 && render.scale.outWrite==base+6*pitch);
 }
 SDL_DestroyWindow(sdl.window);SDL_Quit();
}
'''
path=work/'consumer.cpp';path.write_text(source)
flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl2'],text=True).split()
subprocess.run(['c++','-std=c++14','-Wall','-Wextra',str(path),'-o',str(work/'consumer'),*flags],check=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy')
for action in ('save','load'):
 subprocess.run([str(work/'consumer'),action,str(work/'render.bin')],env=env,check=True)
print('Production Render + real SDL dummy surface fresh-process RGB/cursor/history -> aspect2/3 acquired hit-hit-miss no reacquisition PASS')

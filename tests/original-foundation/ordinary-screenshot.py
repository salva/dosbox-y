#!/usr/bin/env python3
"""Actual ordinary CAPTURE_AddImage screenshot branch, shared PNG ownership."""
import os
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
src=(root/'src/hardware/hardware.cpp').read_text()
a=src.index('\tif (CaptureState & CAPTURE_IMAGE) {',src.index('void CAPTURE_AddImage('))
b=src.index('skip_shot:',a)
branch=src[a:b]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'ordinary-screenshot'
work.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as d:
 p=Path(d)
 (p/'probe.cpp').write_text('''#include <cassert>
#include <string>
#include <vector>
#include <unistd.h>
#include "capture_png.h"
using Bitu=unsigned long;
#define C_SDL2 1
#define VERSION "test"
#define LOG_MSG(...) ((void)0)
constexpr unsigned CAPTURE_IMAGE=1,CAPTURE_FLAG_DBLH=2,CAPTURE_FLAG_DBLW=4;
unsigned CaptureState=0, notices=0;
bool show_recorded_filename=true,fail=false;
std::string pathscr;
FILE *OpenCaptureFile(const char*,const char*){pathscr="shot.png";return fopen(fail?"/dev/full":pathscr.c_str(),"wb");}
void systemmessagebox(const char*,const char*,const char*,const char*,int){++notices;}
void shot(Bitu width,Bitu height,Bitu bpp,Bitu pitch,Bitu flags,uint8_t*data,uint8_t*pal){
 Bitu countWidth=width;
 if(flags&CAPTURE_FLAG_DBLH)height*=2;
 if(flags&CAPTURE_FLAG_DBLW)width*=2;
'''+branch+'''
skip_shot:;
}
int main(){
 uint8_t data[16*16*4],pal[1024];
 for(unsigned i=0;i<sizeof(data);++i)data[i]=i*17;
 for(unsigned i=0;i<sizeof(pal);++i)pal[i]=i;
 for(unsigned bpp:{8u,15u,16u,32u})for(unsigned flags:{0u,2u,4u,6u}){
  auto n=notices;CaptureState=CAPTURE_IMAGE;
  shot(16,16,bpp,64,flags,data,pal);
  assert(!CaptureState&&pathscr.empty()&&notices==n+1);
  png_image image={};image.version=PNG_IMAGE_VERSION;
  assert(png_image_begin_read_from_file(&image,"shot.png"));
  assert(image.width==16*((flags&4)?2:1)&&image.height==16*((flags&2)?2:1));
  image.format=PNG_FORMAT_RGB;std::vector<uint8_t> out(PNG_IMAGE_SIZE(image));
  assert(png_image_finish_read(&image,nullptr,out.data(),0,nullptr));png_image_free(&image);
  unsigned w=16*((flags&4)?2:1),h=16*((flags&2)?2:1);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
   const auto*in=data+(y/((flags&2)?2:1))*64+(x/((flags&4)?2:1))*(bpp==8?1:bpp==32?4:2);
   uint8_t r,g,b;
   if(bpp==8){r=pal[*in*4];g=pal[*in*4+1];b=pal[*in*4+2];}
   else if(bpp==32){r=in[2];g=in[1];b=in[0];}
   else{uint16_t v;memcpy(&v,in,2);b=((v&31)*33)>>2;g=bpp==15?((v&0x3e0)*33)>>7:((v&0x7e0)*65)>>9;r=bpp==15?((v&0x7c00)*33)>>12:((v&0xf800)*33)>>13;}
   auto*o=out.data()+(y*w+x)*3;assert(o[0]==r&&o[1]==g&&o[2]==b);
  }
 }
 auto n=notices;fail=true;CaptureState=CAPTURE_IMAGE;
 shot(16,16,32,64,0,data,pal);assert(notices==n&&!CaptureState&&pathscr.empty());
 assert(access("shot.png",F_OK)!=0);
 fail=false;CaptureState=CAPTURE_IMAGE;
 shot(16,16,24,64,0,data,pal);assert(notices==n&&access("shot.png",F_OK)!=0);
}
''')
 flags=subprocess.check_output(['pkg-config','--cflags','--libs','libpng'],text=True).split()
 subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined','-I'+str(root/'src/hardware'),str(p/'probe.cpp'),'-o',str(p/'probe'),*flags],check=True)
 subprocess.run([str(p/'probe')],cwd=p,check=True)
print('Ordinary screenshot actual branch all bpp/doubling decoded pixels + /dev/full + invalid output sanitizer PASS')

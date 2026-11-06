#!/usr/bin/env python3
"""Production PS output branch, real stdio /dev/full publication refusal."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'src/hardware/parport/printer.cpp').read_text()
a=s.index('\n\telse if (strcasecmp(output, "ps") == 0)',s.index('bool CPrinter::outputPage'))
b=s.index('\n\telse\n',a)
branch=s[a:b].replace('else if (','if (',1)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'printer-publication';w.mkdir(exist_ok=True)
p=w/'probe.cpp'
p.write_text('''#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdint>
struct Surface{int w=8,h=8;}surface;
void SDL_LockSurface(Surface*){}void SDL_UnlockSurface(Surface*){}
#define LOG(a,b) printf
#define LOG_MISC 0
#define LOG_ERROR 0
struct CPrinter {const char*output="ps";Surface*page=&surface;
 bool multipageOutput=false;void*outputHandle=nullptr;
 double defaultPageWidth=8,defaultPageHeight=11;int multiPageCounter=0;
 int ASCII85BufferPos=0,ASCII85CurCol=0,actions=0;
 const char*target="/dev/full";
 void findNextName(const char*,const char*,char*p){strcpy(p,target);}
 uint8_t getPixel(uint32_t){return 0;}
 void fprintASCII85(FILE*f,uint16_t b){fprintf(f,"%u",unsigned(b));}
 void doAction(const char*){++actions;}
 bool outputPage(){char fname[256]{};
'''+branch+'''
return true;}};
int main(){
 CPrinter p;assert(!p.outputPage());assert(!p.outputHandle&&!p.actions);
 p.multipageOutput=true;assert(!p.outputPage());assert(!p.outputHandle&&!p.actions);
 p.target="/dev/null";p.multipageOutput=false;assert(p.outputPage());assert(p.actions==1);
 p.multipageOutput=true;assert(p.outputPage());assert(p.outputHandle);fclose((FILE*)p.outputHandle);
}
''')
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(p),'-o',str(w/'probe')],check=True)
subprocess.run([str(w/'probe')],check=True)
print('Actual PS branch real stdio full/null single/multipage refusal/action/close ASan/UBSan PASS')
# Exercise the actual PNG publication tail with real libpng and stdio errors.
a=s.index('        // All C++ image storage is already constructed')
b=s.index('\n\t}\n#endif',a)
png=s[a:b]
q=w/'png.cpp'
q.write_text('''#include <png.h>
#include <cstdio>
#include <cassert>
#include <vector>
int actions=0;const char*fname="unused";
void doAction(const char*){++actions;}
bool publish(const char*target){
 FILE*fp=fopen(target,"wb");assert(fp);
 png_structp png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
 png_infop info_ptr=png_create_info_struct(png_ptr);
 png_init_io(png_ptr,fp);png_set_IHDR(png_ptr,info_ptr,8,8,8,PNG_COLOR_TYPE_RGB,
 PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
 std::vector<unsigned char> image(8*8*3,42);
 std::vector<png_bytep> row_pointers(8);
 for(unsigned y=0;y<8;++y)row_pointers[y]=image.data()+y*24;
'''+png+'''
return true;}
int main(){assert(!publish("/dev/full"));assert(!actions);
 assert(publish("/dev/null"));assert(actions==1);}
''')
flags=subprocess.check_output(['pkg-config','--cflags','--libs','libpng'],text=True).split()
subprocess.run(['c++','-std=c++14','-fsanitize=address,undefined',str(q),'-o',str(w/'png')]+flags,check=True)
subprocess.run([str(w/'png')],check=True)
print('Actual PNG publication tail real libpng/stdout full/null refusal/close ASan/UBSan PASS')

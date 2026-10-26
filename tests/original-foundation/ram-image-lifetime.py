#!/usr/bin/env python3
"""Actual RAM initializer/destructor backing ownership, including failed map."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/ints/bios_memdisk.cpp').read_text()
a = s.index('void imageDiskMemory::init(')
# Keep exact complete function bodies, without depending on following comments.
def body(start):
    left = s.index('{', start)
    depth = 1
    end = left + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
code = r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define LOG_MSG(...) ((void)0)
struct diskGeo {
 uint32_t cylcount=2,headscyl=2,secttrack=2,bytespersect=512;
};
struct imageDisk {
 int refs=1;
 void Addref(){++refs;}
 void Release(){--refs;}
};
bool fail_map=false;
void* alloc_map(size_t n){return fail_map?nullptr:std::malloc(n);}
struct imageDiskMemory {
 uint32_t total_sectors=0,total_chunks=0,sectors_per_chunk=0,chunk_size=0;
 uint8_t** ChunkMap=nullptr;imageDisk* underlyingImage=nullptr;
 bool active=false,hardDrive=false;
 const char* diskname=nullptr;
 uint32_t heads=0,cylinders=0,sectors=0,sector_size=0,reserved_cylinders=0;
 uint64_t diskSizeK=0;diskGeo floppyInfo;
 void init(diskGeo,bool,imageDisk*);
 ~imageDiskMemory();
};
#define malloc alloc_map
INIT
#undef malloc
DISPOSE
int main(){
 imageDisk backing;
 {imageDiskMemory image;diskGeo g;g.cylcount=0;image.init(g,true,&backing);
  assert(!image.active && backing.refs==1);}
 assert(backing.refs==1);
 {imageDiskMemory image;diskGeo g;g.cylcount=0xffffffff;g.headscyl=0xffffffff;
  image.init(g,true,&backing);assert(!image.active && backing.refs==1);}
 assert(backing.refs==1);
 fail_map=true;
 {imageDiskMemory image;image.init(diskGeo{},true,&backing);
  assert(!image.active && backing.refs==1);}
 assert(backing.refs==1);fail_map=false;
 {imageDiskMemory image;image.init(diskGeo{},true,&backing);
  assert(image.active && backing.refs==2);
  image.ChunkMap[0]=static_cast<uint8_t*>(std::malloc(image.chunk_size));}
 assert(backing.refs==1);
 {imageDiskMemory image;image.init(diskGeo{},false,nullptr);assert(image.active);}
}
'''.replace('INIT', body(a)).replace('DISPOSE', body(s.index('imageDiskMemory::~imageDiskMemory()')))
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'ram-image-lifetime'
w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual RAM init/disposal backing reference ASan/UBSan PASS; not save/reconstruction proof')

#!/usr/bin/env python3
"""Actual RAM image sector writer; backing read errors and chunk identity."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'src/ints/bios_memdisk.cpp').read_text()
a=s.index('Int13Status imageDiskMemory::Write_AbsoluteSector(')
b=s.index('// Partition and format the ramdrive',a)
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'ram-image-write';w.mkdir(exist_ok=True)
code=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define LOG_MSG(...) ((void)0)
enum class Int13Status {NoError,SectorNotFound,ControllerFailure};
struct imageDisk {virtual ~imageDisk()=default;virtual Int13Status Read_AbsoluteSector(uint32_t,void*)=0;};
struct Backing:imageDisk {
 int fail=-1;
 Int13Status Read_AbsoluteSector(uint32_t n,void*p) override {
  if(int(n)==fail)return Int13Status::ControllerFailure;
  memset(p,int(n+1),8);return Int13Status::NoError;
 }
};
struct imageDiskMemory {
 uint32_t total_sectors=6,sectors_per_chunk=4,chunk_size=32,total_chunks=2,sector_size=8;
 uint8_t* ChunkMap[2]={};imageDisk* underlyingImage=nullptr;
 ~imageDiskMemory(){for(auto p:ChunkMap)free(p);}
 Int13Status Write_AbsoluteSector(uint32_t,const void*);
};
BODY
int main(){
 uint8_t bytes[8];memset(bytes,99,sizeof(bytes));
 for(int fail:{0,1,2,3}){
  Backing backing;backing.fail=fail;imageDiskMemory disk;disk.underlyingImage=&backing;
  assert(disk.Write_AbsoluteSector(2,bytes)==Int13Status::ControllerFailure);
  assert(!disk.ChunkMap[0]);
  backing.fail=-1;assert(disk.Write_AbsoluteSector(2,bytes)==Int13Status::NoError);
  for(unsigned i=0;i<32;++i)assert(disk.ChunkMap[0][i]==(i/8==2?99:i/8+1));
 }
 {Backing backing;imageDiskMemory disk;disk.underlyingImage=&backing;
  assert(disk.Write_AbsoluteSector(5,bytes)==Int13Status::NoError);
  for(unsigned i=0;i<16;++i)assert(disk.ChunkMap[1][i]==(i/8==1?99:5));
  assert(disk.Write_AbsoluteSector(6,bytes)==Int13Status::SectorNotFound);
 }
 {imageDiskMemory disk;uint8_t zero[8]={};
  assert(disk.Write_AbsoluteSector(0,zero)==Int13Status::NoError);assert(!disk.ChunkMap[0]);
  assert(disk.Write_AbsoluteSector(3,bytes)==Int13Status::NoError);
  for(unsigned i=0;i<32;++i)assert(disk.ChunkMap[0][i]==(i>=24?99:0));
 }
}
'''.replace('BODY',s[a:b]).replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual RAM image writer backing/error/chunk ASan/UBSan PASS; not saved RAM image qualification')

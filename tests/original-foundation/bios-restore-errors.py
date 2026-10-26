#!/usr/bin/env python3
"""Actual consumed BIOS reconstruction branches with lifetime doubles, not devices."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('        if (imageDiskList[d])\n            ;')
b = s.index('        if (i==MAX_DISK_IMAGES-1)', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'bios-restore-errors'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
using Bitu=unsigned;
int mode=0,retired=0,attached=0;
struct imageDisk { virtual ~imageDisk(){++retired;} };
struct imageDiskElToritoFloppy:imageDisk {
 imageDiskElToritoFloppy(unsigned char,unsigned,unsigned){}
};
struct imageDiskMemory:imageDisk {int Format(){return mode==2?1:0;}};
imageDiskMemory* CreateRamDrive(Bitu*,int,bool,void*){return mode==1?nullptr:new imageDiskMemory;}
struct imageDiskVHD {enum {OPEN_SUCCESS=0};static int Open(const char*,bool,imageDisk** p){
 if(mode==1)return 1;*p=new imageDisk;return 0;
}};
struct fatDrive {
 bool created_successfully;imageDisk* loadedDisk;
 fatDrive(const char*,unsigned,unsigned,unsigned,unsigned,const std::vector<std::string>&):
 created_successfully(mode!=1),loadedDisk(mode==1?nullptr:new imageDisk){}
 ~fatDrive(){delete loadedDisk;}
};
imageDisk* imageDiskList[6]={};
void E_Exit(const char* text){throw std::runtime_error(text);}
bool AttachToBiosByIndex(imageDisk* p,unsigned d){
 assert(d==4);if(mode==3)return false;++attached;imageDiskList[d]=p;return true;
}
bool AttachToBiosAndIdeByIndex(imageDisk* p,unsigned d,unsigned char,bool){
 return AttachToBiosByIndex(p,d);
}
struct Options {unsigned mounttype=0,CDROM_drive=0,cdrom_sector_offset=0,floppy_emu_type=0;
 unsigned bytesector=0,cylsector=0,headscyl=0,cylinders=0;}opts;
void restore(){int d=4;Bitu sizes[4]={};int ide_index=-1;bool ide_slave=false;
 char diskname[]="image";
 BRANCH
}
int main(){
 // Failure must not return completion. FAT retains its loaded image through
 // the drive lifetime in this double; test failures only for that branch.
 for(unsigned type:{0u,1u,2u,3u,4u})for(int failure:{1,2,3}){
  if(type==0 && failure==2)continue;
  if(type==1 && failure!=3)continue;
  if(type==3 && failure==2)continue;
  opts.mounttype=type;mode=failure;bool fatal=false;int before=attached;
  try{restore();}catch(const std::runtime_error&){fatal=true;}
  assert(fatal && attached==before && !imageDiskList[4]);
 }
 for(unsigned type:{1u,2u,3u}){
  opts.mounttype=type;mode=0;int before=attached;restore();
  assert(attached==before+1 && imageDiskList[4]);
  delete imageDiskList[4];imageDiskList[4]=nullptr;
 }
}
'''.replace('BRANCH',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('BIOS reconstruction failure/lifetime branches ASan/UBSan PASS; not native device qualification')

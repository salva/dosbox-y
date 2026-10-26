#!/usr/bin/env python3
"""Exercise actual BIOS attachment primitive with refcounted lifetime doubles."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root / 'src/dos/dos_programs.cpp').read_text()
a = s.index('bool AttachToBiosByIndex(')
b = s.index('bool AttachToBiosByLetter(', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'bios-image-attachment'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <cstddef>
const unsigned MAX_DISK_IMAGES=6;
int retired=0, detached=0, fdc=0, fdd=0, dpt=0;
struct imageDisk {
 int refs=0;
 void Addref(){++refs;}
 void Release(){assert(refs>0);if(!--refs){++retired;delete this;}}
};
imageDisk *imageDiskList[MAX_DISK_IMAGES]={};
bool imageDiskChange[MAX_DISK_IMAGES]={};
void IDE_Hard_Disk_Detach(unsigned){++detached;}
void FDC_AssignINT13Disk(unsigned){++fdc;}
void incrementFDD(){++fdd;}
void updateFloppyDPT(){++dpt;}
void IDE_Hard_Disk_Attach(signed char, bool, unsigned){}
void updateDPT(){}
PRIMITIVE
int main(){
 auto incoming=new imageDisk;
 assert(!AttachToBiosByIndex(incoming,MAX_DISK_IMAGES));
 assert(incoming->refs==0 && retired==0);
 assert(AttachToBiosByIndex(incoming,2));
 assert(incoming->refs==1 && imageDiskList[2]==incoming);
 // Exact same sole native resource must survive reattachment.
 assert(AttachToBiosByIndex(incoming,2));
 assert(incoming->refs==1 && retired==0 && detached==1);
 auto replacement=new imageDisk;
 assert(AttachToBiosByIndex(replacement,2));
 assert(retired==1 && replacement->refs==1 && detached==2);
 assert(AttachToBiosByIndex(replacement,0));
 assert(replacement->refs==2 && fdc==1 && fdd==1 && dpt==1);
 imageDiskList[2]->Release();imageDiskList[2]=nullptr;
 imageDiskList[0]->Release();imageDiskList[0]=nullptr;
 assert(retired==2);
 // Restore a later saved BIOS slot even when an earlier slot is free.
 auto saved=new imageDisk;
 assert(AttachToBiosAndIdeByIndex(saved,4,1,false));
 assert(imageDiskList[2]==nullptr && imageDiskList[4]==saved);
 assert(saved->refs==1);
 imageDiskList[4]->Release();imageDiskList[4]=nullptr;
 assert(retired==3);
}
'''.replace('PRIMITIVE',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('BIOS native attachment refcount/ordering ASan/UBSan PASS; not live image restore qualification')

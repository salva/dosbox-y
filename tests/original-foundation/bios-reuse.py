#!/usr/bin/env python3
"""Consumed BIOS image kind/geometry reuse; extracted decision, not device proof."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
s = (root/'src/dos/dos_files.cpp').read_text()
a = s.index('        const auto *torito =')
b = s.index('        if (reuse_image)', a)
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'bios-reuse'
w.mkdir(exist_ok=True)
code = r'''
#include <cassert>
#include <string>
#include <strings.h>
struct imageDisk {virtual ~imageDisk()=default;std::string diskname="image";unsigned sector_size=512,sectors=18,heads=2,cylinders=80;};
struct imageDiskMemory:imageDisk {};
struct imageDiskVHD:imageDisk {};
struct imageDiskElToritoFloppy:imageDisk {unsigned CDROM_drive=2,cdrom_sector_offset=17,floppy_type=3;};
struct Options {unsigned mounttype=1,CDROM_drive=2,cdrom_sector_offset=17,floppy_emu_type=3,bytesector=512,cylsector=18,headscyl=2,cylinders=80;}opts;
imageDisk* imageDiskList[6]={};
bool reuse(){const int d=4;const char* diskname="image";
DECISION
return reuse_image;
}
int main(){
 assert(!reuse());
 imageDisk other;imageDiskList[4]=&other;assert(!reuse());
 imageDiskElToritoFloppy torito;imageDiskList[4]=&torito;assert(reuse());
 ++opts.CDROM_drive;assert(!reuse());--opts.CDROM_drive;
 ++opts.cdrom_sector_offset;assert(!reuse());--opts.cdrom_sector_offset;
 ++opts.floppy_emu_type;assert(!reuse());--opts.floppy_emu_type;assert(reuse());
 // Same pathname is not authority to reuse a different native image kind.
 opts.mounttype=0;imageDiskList[4]=&torito;assert(!reuse());
 imageDiskList[4]=&other;assert(reuse());
 ++opts.bytesector;assert(!reuse());--opts.bytesector;
 ++opts.cylsector;assert(!reuse());--opts.cylsector;
 ++opts.headscyl;assert(!reuse());--opts.headscyl;
 ++opts.cylinders;assert(!reuse());--opts.cylinders;
 imageDiskMemory ram;imageDiskVHD vhd;
 imageDiskList[4]=&ram;assert(!reuse());
 opts.mounttype=2;assert(reuse());
 imageDiskList[4]=&other;assert(!reuse());
 imageDiskList[4]=&vhd;assert(!reuse());
 opts.mounttype=3;assert(reuse());
 imageDiskList[4]=&ram;assert(!reuse());
 opts.mounttype=4;imageDiskList[4]=&other;assert(!reuse());
 opts.mounttype=0;
 other.diskname="different";assert(!reuse());
}
'''.replace('DECISION',s[a:b])
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('Actual BIOS image kind/geometry reuse decision ASan/UBSan PASS; not media/device qualification')

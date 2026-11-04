#!/usr/bin/env python3
"""Actual DriveManager cursor consumer; no mounted drive runtime claim."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'drive-consumption';work.mkdir(exist_ok=True)
t=(root/'src/dos/drives.cpp').read_text();start=t.index('void DriveManager::LoadState(');end=t.index('\nvoid POD_Save_DOS_DriveManager',start)
source=r'''
#include <cassert>
#include <sstream>
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
constexpr int DOS_DRIVES=26;
struct DriveManager {static int currentDrive;static void LoadState(std::istream&);};
int DriveManager::currentDrive=0;
'''+t[start:end]+r'''
int main(){
 for(int v=-2;v<=28;++v){std::istringstream s(std::string((char*)&v,sizeof(v)));
 DriveManager::currentDrive=7;DriveManager::LoadState(s);
 bool valid=v>=0 && v<DOS_DRIVES;assert(bool(s)==valid);
 assert(DriveManager::currentDrive==(valid?v:7));}
 for(unsigned n=0;n<sizeof(int);++n){std::istringstream s(std::string(n,'\0'));
 DriveManager::currentDrive=7;DriveManager::LoadState(s);assert(s.fail() && DriveManager::currentDrive==7);}
}
'''
p=work/'consumer.cpp';p.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('DriveManager cursor: native bounds and truncations passed')

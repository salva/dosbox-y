#!/usr/bin/env python3
"""External-device classification must not poll internal device status.

Extract the actual owner chain walker; CON-like status intentionally throws.
The full startup GDB consumer is in fresh-startup-no-replay.py.
"""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'external-device-classification'
work.mkdir(exist_ok=True)
source = (root / 'src/dos/dos_devices.cpp').read_text()
body = source[source.index('uint32_t DOS_CheckExtDevice('):
              source.index('static void DOS_CheckOpenExtDevice(')]
fixture = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
constexpr unsigned DOS_DEVICES=4;
struct DOS_Device { virtual ~DOS_Device()=default;
 virtual uint16_t GetInformation(){throw "guest dispatch";} };
struct DOS_ExtDevice: DOS_Device {
 uint16_t segment=0x100,strategy=1,interrupt=2;
 bool CheckSameDevice(uint16_t s,uint16_t a,uint16_t b){
  return s==segment && a==strategy && b==interrupt;
 }
};
DOS_Device *Devices[DOS_DEVICES]{};
struct { uint32_t GetStartOfDeviceChain(){return 0x1000000;} } dos_infoblock;
uint8_t memory[64]{};
uint8_t real_readb(uint16_t s,uint16_t o){assert(s==0x100 && o<64);return memory[o];}
uint16_t real_readw(uint16_t s,uint16_t o){return real_readb(s,o)|(real_readb(s,o+1)<<8);}
uint32_t real_readd(uint16_t s,uint16_t o){return real_readw(s,o)|(uint32_t(real_readw(s,o+2))<<16);}
''' + body + r'''
int main(){
 DOS_Device con;DOS_ExtDevice ext;
 Devices[0]=&con; // Classification must not call GetInformation.
 memory[0]=memory[1]=memory[2]=memory[3]=255;
 memory[5]=0x80; memory[6]=1;memory[8]=2;
 memcpy(memory+10,"CON     ",8);
 assert(DOS_CheckExtDevice("CON",true)==0x1000000);
 assert(DOS_CheckExtDevice("OTHER",true)==0);
 Devices[1]=&ext;
 assert(DOS_CheckExtDevice("CON",true)==0); // Already represented driver.
 assert(DOS_CheckExtDevice("CON",false)==0x1000000);
 ext.strategy=3;
 assert(DOS_CheckExtDevice("CON",true)==0x1000000);
 memset(memory+6,0,4); // Native default CON/NUL chain entries are not external.
 assert(DOS_CheckExtDevice("CON",true)==0);
}
'''
(work / 'test.cpp').write_text(fixture)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                str(work / 'test.cpp'), '-o', str(work / 'test')], check=True)
subprocess.run([str(work / 'test')], check=True)
print('Actual external-device chain classification: internal status never polled; '
      'external identity/default chain cases passed ASan/UBSan')

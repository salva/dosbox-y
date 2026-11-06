#!/usr/bin/env python3
"""Actual CMS and Tandy MAME chip active synthesis continuation, no alternate codec."""
import pathlib,os,subprocess
r=pathlib.Path(__file__).resolve().parents[2]
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mame-active';w.mkdir(exist_ok=True)
code=r'''
#include "emu.h"
#include "saa1099.h"
#include "sn76496.h"
#include <cassert>
#include <sstream>
#include <cstring>
#include <algorithm>
void LOG::operator()(const char*,...) {}
struct TestPSG:sn76496_device {
 using sn76496_device::sn76496_device;
 using sn76496_base_device::device_start;
 using sn76496_base_device::sound_stream_update;
};
int main(){
 machine_config c;
 saa1099_device cms(c,nullptr,nullptr,7159090),fresh(c,nullptr,nullptr,7159090);
 cms.device_start();fresh.device_start();
 auto reg=[&](unsigned n,unsigned v){cms.control_w(0,0,n);cms.data_w(0,0,v);};
 reg(0,0xff);reg(8,0x83);reg(16,0x33);reg(20,1);reg(21,1);reg(22,2);reg(24,0x8e);reg(28,1);
 device_sound_interface::sound_stream stream;
 int16_t a[2][256]={},b[2][256]={};int16_t *ap[]={a[0],a[1]},*bp[]={b[0],b[1]};
 cms.sound_stream_update(stream,nullptr,ap,256);
 assert(std::any_of(a[0],a[0]+256,[](int16_t n){return n!=0;}));
 std::ostringstream saved;cms.SaveState(saved);assert(saved);
 std::istringstream input(saved.str());fresh.LoadState(input);assert(input);
 cms.sound_stream_update(stream,nullptr,ap,256);fresh.sound_stream_update(stream,nullptr,bp,256);
 assert(!memcmp(a,b,sizeof(a)));
 std::ostringstream again;fresh.SaveState(again);assert(again);
 for(size_t n=0;n<saved.str().size();++n){std::istringstream shortin(saved.str().substr(0,n));fresh.LoadState(shortin);assert(!shortin);}
 TestPSG psg(c,nullptr,nullptr,3579545),other(c,nullptr,nullptr,3579545);
 psg.device_start();other.device_start();psg.write(0x85);psg.write(0x10);psg.write(0x90);psg.write(0xe4);psg.write(0xf2);
 psg.sound_stream_update(stream,nullptr,ap,256);
 assert(std::any_of(a[0],a[0]+256,[](int16_t n){return n!=0;}));
 std::ostringstream history;psg.SaveState(history);std::istringstream load(history.str());other.LoadState(load);assert(load);
 psg.sound_stream_update(stream,nullptr,ap,256);other.sound_stream_update(stream,nullptr,bp,256);
 assert(!memcmp(a[0],b[0],sizeof(a[0])));
 for(size_t n=0;n<history.str().size();++n){std::istringstream shortin(history.str().substr(0,n));other.LoadState(shortin);assert(!shortin);}
}
'''
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'build-lua'),'-I'+str(r/'include'),'-I'+str(r/'src/hardware/mame'),str(w/'test.cpp'),str(r/'src/hardware/mame/saa1099.cpp'),str(r/'src/hardware/mame/sn76496.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual active CMS stereo and Tandy PSG noise/phase continuation plus every truncation passed')

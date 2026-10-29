#!/usr/bin/env python3
"""Compile the actual DBOPL owner; check roles before native reconstruction.
Mixer output is unused: tests call the original chip generator directly.
"""
import os,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
work=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dbopl-consumption'
work.mkdir(exist_ok=True)
work=pathlib.Path(tempfile.mkdtemp(prefix='run-',dir=work))
config=pathlib.Path(os.environ['DOSBOX_TEST_CONFIG_DIR'])
source=r'''
#include <math.h>
#include "dosbox.h"
#include "dbopl.h"
#include "mixer.h"
#include <cassert>
#include <cstring>
#include <sstream>
bool adlib_pcm_boost=false;
void adlib_boost32(int32_t*,unsigned int,unsigned int) {}
void MixerChannel::AddSamples_m32(Bitu,const int32_t*) {}
void MixerChannel::AddSamples_s32(Bitu,const int32_t*) {}
int main() {
 DBOPL::Handler producer(true);producer.Init(44100);
 std::ostringstream output;producer.SaveState(output);
 const auto good=output.str();
 const size_t chip=32+8192+sizeof(bool), roles=chip+sizeof(DBOPL::Chip);
 assert(good.size()==roles+36+36*sizeof(Bitu)+18);
 for (int fault=0;fault<13;++fault) {
  auto bytes=good;
  switch(fault) {
   case 0:bytes.replace(0,32,32,'X');break;
   case 1:bytes[roles]=char(255);break;
   case 2:std::memset(&bytes[roles+36],255,sizeof(Bitu));break;
   case 3:bytes[roles+36+36*sizeof(Bitu)]=char(255);break;
   case 4:bytes.resize(roles);break;
   case 5: {Bitu offset=1;std::memcpy(&bytes[roles+36],&offset,sizeof(offset));break;}
   case 6: {uint32_t mask=0xffffffff;
    const size_t at=chip+offsetof(DBOPL::Chip,chan)+offsetof(DBOPL::Channel,op)+offsetof(DBOPL::Operator,waveMask);
    std::memcpy(&bytes[at],&mask,sizeof(mask));break;}
   case 7:bytes[chip+offsetof(DBOPL::Chip,vibratoIndex)]=32;break;
   case 8:bytes[chip+offsetof(DBOPL::Chip,tremoloIndex)]=char(255);break;
   case 9:bytes[chip+offsetof(DBOPL::Chip,vibratoStrength)]=char(255);break;
   case 10:bytes[chip+offsetof(DBOPL::Chip,tremoloStrength)]=char(255);break;
   case 11:std::memset(&bytes[chip+offsetof(DBOPL::Chip,lfoAdd)],0,sizeof(uint32_t));break;
   case 12:std::memset(&bytes[chip+offsetof(DBOPL::Chip,lfoCounter)],255,sizeof(uint32_t));break;
  }
  DBOPL::Handler target(true);target.Init(44100);
  std::istringstream input(bytes);target.LoadState(input);assert(input.fail());
 }
 DBOPL::Handler target(true);target.Init(44100);
 std::istringstream input(good);target.LoadState(input);assert(input);
 int32_t a[32]={},b[32]={};producer.chip.GenerateBlock2(32,a);target.chip.GenerateBlock2(32,b);
 assert(!std::memcmp(a,b,sizeof(a)));
}
'''
p=work/'consumer.cpp';p.write_text(source)
cmd=['c++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
     '-I'+str(config),'-I'+str(root/'include'),'-I'+str(root/'src/hardware'),
     str(p),str(root/'src/hardware/dbopl.cpp'),'-o',str(work/'consumer')]
subprocess.run(cmd,check=True)
subprocess.run([str(work/'consumer')],check=True)
print(work,'actual DBOPL marker/truncation/volume/wave-span-alignment/synth rejection and native chip roundtrip passed')

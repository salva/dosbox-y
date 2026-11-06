#!/usr/bin/env python3
"""Actual Tandy DMA producer: bounded chunks and synchronous completion."""
import os
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
source = (root / 'src/hardware/tandy_sound.cpp').read_text()
body = source.split('static void TandyDACGenerateDMASound(Bitu length) {', 1)[1].split('\nstatic void TandyDACUpdate', 1)[0]
work = pathlib.Path(tempfile.mkdtemp(prefix='tandy-dma-', dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>
using Bitu = size_t;
constexpr Bitu TDAC_DMA_BUFSIZE = 16;
struct Channel { std::vector<uint8_t> samples; void AddSamples_m8(Bitu n,uint8_t *p) {samples.insert(samples.end(),p,p+n);} };
struct DMA { Bitu calls=0, mode=0; Bitu Read(Bitu n,uint8_t *p); };
struct {struct {Channel *chan; struct {DMA *chan; uint8_t buf[TDAC_DMA_BUFSIZE], last_sample; bool transfer_done;} dma;} dac;} tandy;
Bitu DMA::Read(Bitu n,uint8_t *p) {
 assert(n<=TDAC_DMA_BUFSIZE); ++calls;
 Bitu count=mode==1?n/2:n;
 for(Bitu i=0;i<count;++i)p[i]=uint8_t(calls*10+i);
 if(mode==2)tandy.dac.dma.transfer_done=true;
 return count;
}
static void TandyDACGenerateDMASound(Bitu length) {
'''+body+r'''
int main() {
 for(Bitu mode=0;mode<3;++mode)for(Bitu n : {Bitu(0),Bitu(1),Bitu(16),Bitu(17),Bitu(65)}) {
  Channel out; DMA dma; dma.mode=mode;
  tandy.dac.chan=&out;tandy.dac.dma.chan=&dma;
  tandy.dac.dma.transfer_done=false;tandy.dac.dma.last_sample=128;
  TandyDACGenerateDMASound(n);
  assert(out.samples.size()==n);
  assert(dma.calls==(mode==2?Bitu(n!=0):(n+15)/16));
  if(mode==2&&n>16)for(Bitu i=16;i<n;++i)assert(out.samples[i]==out.samples[15]);
  if(mode==1&&n>=16)for(Bitu i=8;i<16;++i)assert(out.samples[i]==out.samples[7]);
 }
}
'''
(work/'test.cpp').write_text(code)
subprocess.run(['g++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g',str(work/'test.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Actual Tandy DMA producer zero/boundary/large/short/synchronous completion ASan/UBSan PASS')

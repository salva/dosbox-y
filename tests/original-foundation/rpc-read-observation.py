#!/usr/bin/env python3
"""Actual RPC read primitive routing; observer owners have separate regressions."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'rpc-read-observation'
work.mkdir(exist_ok=True)
text = (root / 'src/agent/debugger/debugger_adapter.cpp').read_text()
start = text.index('bool ReadResolvedByte(')
body = text[start:text.index('\n}\n', start) + 3]
source = r'''
#include <cassert>
#include <cstdint>
#include <string>
namespace std { using ::uint8_t; using ::uint32_t; }
enum class MemorySpace {Physical,Linear,Segmented};
struct MemoryAddress {MemorySpace space;};
struct MemoryAccessError {std::string reason;uint32_t offset=0;};
constexpr uint32_t MemSize=0x1000000;
unsigned physical=0,linear=0;bool allowed=true;
void SetAccessError(MemoryAccessError* e,const char* r,uint32_t o){if(e){e->reason=r;e->offset=o;}}
bool MEM_InspectPhysicalMemory(uint32_t a,uint32_t n,uint8_t* v){assert(a==42&&n==1);++physical;if(!allowed)return false;*v=7;return true;}
bool PAGING_InspectLinear(uint32_t a,uint32_t n,uint8_t* v){assert(a==42&&n==1);++linear;if(!allowed)return false;*v=8;return true;}
''' + body + r'''
int main(){
 uint8_t value=99;MemoryAccessError error;
 MemoryAddress a{MemorySpace::Physical};
 assert(ReadResolvedByte(a,42,&value,&error)&&value==7&&physical==1&&linear==0);
 allowed=false;value=99;
 assert(!ReadResolvedByte(a,42,&value,&error)&&value==99&&error.reason=="physical_not_inspectable"&&error.offset==42);
 assert(!ReadResolvedByte(a,MemSize,&value,&error)&&physical==2&&error.reason=="physical_out_of_range");
 for(auto space:{MemorySpace::Linear,MemorySpace::Segmented}){
  a.space=space;allowed=true;
  assert(ReadResolvedByte(a,42,&value,&error)&&value==8);
  allowed=false;value=99;
  assert(!ReadResolvedByte(a,42,&value,&error)&&value==99&&error.reason=="linear_not_inspectable"&&error.offset==42);
 }
 assert(linear==4&&physical==2);
 assert(!ReadResolvedByte(a,42,&value,nullptr));
}
'''
(work / 'read.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'read.cpp'),'-o',str(work/'read')],check=True)
subprocess.run([str(work/'read')],check=True)
print('RPC physical/linear/segmented read observer routing PASS; admission/lifetime not qualified')

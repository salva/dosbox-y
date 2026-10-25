#!/usr/bin/env python3
"""Exercise actual RPC read publication; memory/authority are doubles."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'rpc-read-publication'
work.mkdir(exist_ok=True)
text=(root/'src/agent/debugger/debugger_adapter.cpp').read_text()
start=text.index('bool DebuggerAdapter::ReadMemory(')
body=text[start:text.index('\n}\n',start)+3]
source=r'''
#include <cassert>
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#define C_DEBUG 1
struct MemoryAddress {};
struct MemoryAccessError { unsigned failure=0; };
bool available=true,thread=true;
unsigned fail_resolve=99,fail_read=99;
bool RequireEmulationThread(std::string*) {return thread;}
bool ResolveMemoryByte(const MemoryAddress&,std::size_t i,uint32_t *r,MemoryAccessError *e){
 if(i==fail_resolve){if(e)e->failure=1;return false;} *r=i;return true;
}
bool ReadResolvedByte(const MemoryAddress&,uint32_t r,uint8_t *v,MemoryAccessError *e){
 if(r==fail_read){if(e)e->failure=2;return false;} *v=uint8_t(r+10);return true;
}
struct DebuggerAdapter {
 bool RequireAvailable(std::string*) const{return available;}
 bool ReadMemory(const MemoryAddress&,std::size_t,std::vector<uint8_t>*,MemoryAccessError*,std::string*) const;
};
'''+body+r'''
int main(){
 DebuggerAdapter adapter;MemoryAddress a;MemoryAccessError e;std::string error;
 const std::vector<uint8_t> original{0xaa,0xbb};auto out=original;
 for(unsigned i=0;i<4;++i){
 fail_resolve=i;assert(!adapter.ReadMemory(a,4,&out,&e,&error));assert(out==original&&e.failure==1);
 fail_resolve=99;fail_read=i;assert(!adapter.ReadMemory(a,4,&out,&e,&error));assert(out==original&&e.failure==2);fail_read=99;
 }
 assert(adapter.ReadMemory(a,4,&out,&e,&error));assert((out==std::vector<uint8_t>{10,11,12,13})&&e.failure==0);
 out=original;available=false;assert(!adapter.ReadMemory(a,4,&out,&e,&error)&&out==original);
 available=true;thread=false;assert(!adapter.ReadMemory(a,4,&out,&e,&error)&&out==original);
 thread=true;assert(!adapter.ReadMemory(a,0,&out,&e,&error)&&out==original);
 assert(!adapter.ReadMemory(a,1,nullptr,&e,&error));
}
'''
(work/'publication.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie',str(work/'publication.cpp'),'-o',str(work/'publication')],check=True)
subprocess.run([str(work/'publication')],check=True)
print('RPC read publish-last actual-body PASS; memory effects/attachment lifetime not qualified')

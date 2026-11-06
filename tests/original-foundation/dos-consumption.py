#!/usr/bin/env python3
"""Actual DOS owner codec in reduced layout. No startup/restore claim."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-consumption'
work.mkdir(exist_ok=True)
header=(root/'include/dosbox.h').read_text()
pod=header[header.index('class SerializeGlobalPOD :'):header.index('//---------------- inline implementation')]
dos=(root/'src/dos/dos.cpp').read_text()
codec=dos[dos.index('namespace\n{\n\tclass SerializeDos'):]
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <cstdint>
#define DOS_DRIVES 26
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
class SaveState {public:struct Component {virtual ~Component()=default;virtual void getBytes(std::ostream&)=0;virtual void setBytes(std::istream&)=0;};
 Component *owner=nullptr;static SaveState& instance(){static SaveState s;return s;}
 void registerComponent(const std::string&,Component &c){owner=&c;}};
'''+pod+r'''
struct {uint16_t current=0x200;uint16_t psp(){return current;}uint16_t firstMCB=0,errorcode=0,loaded_codepage=437;
 uint8_t return_code=0,return_mode=0,current_drive=2;
 bool verify=false,breakcheck=true,echo=false,direct_output=true,internal_output=false;
 struct {uint8_t major=6,minor=22;} version;} dos;
char dos_copybuf[32]={};uint16_t countryNo=1;int lfn_filefind_handle=0,bootdrive=3;
bool uselfn=false;
bool dos_kernel_disabled=false;
// Native binding/HMA owners have their own full-consumer probes. Keep them
// admitted and byte-neutral here to isolate this codec's scalar/resource cuts.
struct NativeDOS {bool NativeCallbacksIntact(){return true;}} native_dos;
NativeDOS *test=&native_dos;
void DOS_HMAState(std::ostream*,std::istream*) {}
namespace RestoredSession {bool validRoot(uint16_t root,bool){return root==dos.psp();}}
int resources=0;
#define RESOURCE(name) void POD_Save_DOS_##name(std::ostream &s){s.put('x');}\
 void POD_Load_DOS_##name(std::istream &s){++resources;if(s.get()!='x')s.setstate(std::ios::failbit);}
RESOURCE(Devices) RESOURCE(DriveManager) RESOURCE(Files) RESOURCE(Memory) RESOURCE(Mscdex) RESOURCE(Tables)
'''+codec+r'''
std::string save(){std::ostringstream s;SaveState::instance().owner->getBytes(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);SaveState::instance().owner->setBytes(s);return bool(s);}
int main(){
 const auto bytes=save();dos.verify=true;assert(load(bytes));assert(!dos.verify && dos.breakcheck && dos.direct_output);
 for(size_t n=0;n<bytes.size();++n)assert(!load(bytes.substr(0,n)));
 // Once an owner fails, later resource owners must not run on a failed stream.
 for(size_t n=0;n<6;++n){
   auto bad=bytes;bad[bytes.size()-6+n]='!';resources=0;
   assert(!load(bad) && resources==int(n+1));
 }
 for(size_t n=0;n<6*sizeof(bool);n+=sizeof(bool)){
   auto bad=bytes;bad[n]=2;resources=0;assert(!load(bad));assert(resources==0);
 }
 // External boot must fail before any resource consumer, with no teardown event.
 auto bad=bytes;const bool yes=true;
 memcpy(&bad[bad.size()-6-sizeof(uint16_t)-sizeof(bool)],&yes,sizeof(bool));resources=0;
 assert(!load(bad) && resources==0 && !dos_kernel_disabled);
 bad=bytes;bad[bad.size()-6-sizeof(uint16_t)]^=1;resources=0;
 assert(!load(bad) && resources==0);
 dos.current_drive=DOS_DRIVES;bad=save();resources=0;assert(!load(bad) && resources==0);
 assert(load(bytes));dos_kernel_disabled=true;resources=0;assert(!load(bytes) && resources==0);
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('DOS codec: bool/truncation/default-drive/external-boot refusal before resource consumers passed')

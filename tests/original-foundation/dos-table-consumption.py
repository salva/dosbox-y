#!/usr/bin/env python3
"""Actual DOS table codec with reduced owner layout; no full restore claim."""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
work=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'dos-table-consumption'
work.mkdir(exist_ok=True)
text=(root/'src/dos/dos_tables.cpp').read_text()
source=r'''
#include <cassert>
#include <cstring>
#include <sstream>
#include <cstdint>
#include <list>
struct DOS_GetMemLog_Entry {uint16_t segbase,pages;std::string who;};
std::list<DOS_GetMemLog_Entry> DOS_GetMemLog={{0xc800,4,"dos.tables.tempdta"},{0xc804,4,"dos.tables.fcbdelete"}};
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
struct {struct {uint32_t mediaid=1,tempdta=2,tempdta_fcbdelete=3,dbcs=4,filenamechar=5,collatingseq=6,upcase=7;
 uint16_t dpb=8,dpb_size=0x21,mediaid_offset=0x17;uint8_t *country;} tables;} dos;
uint32_t DOS_TableUpCase=9,DOS_TableLowCase=10,DOS_DriveDataListHead=11;
uint16_t DOS_PRIVATE_SEGMENT=0xc800,DOS_PRIVATE_SEGMENT_END=0xd000;
uint16_t DOS_SDA_SEG=0xb2,DOS_SDA_OFS=0,DOS_SDA_SEG_SIZE=2;
uint16_t DOS_INFOBLOCK_SEG=0x80,DOS_CONDRV_SEG=0xa0,DOS_CDS_SEG=0x108;
uint16_t DOS_IHSEG=0xc810,cmd_line_seg=0xc820;
unsigned call_casemap=12;
constexpr unsigned CB_MAX=128;
using Handler=unsigned(*)();
unsigned DOS_CaseMapFunc(){return 0;}
unsigned wrong_handler(){return 1;}
Handler CallBack_Handlers[CB_MAX]={};
uint8_t ram[0x100000]={};
uint32_t CALLBACK_PhysPointer(unsigned slot){return 0xf0000+slot*32;}
uint8_t mem_readb(uint32_t address){assert(address<sizeof(ram));return ram[address];}
uint16_t mem_readw(uint32_t address){return mem_readb(address)|(uint16_t(mem_readb(address+1))<<8);}
bool DOS_GetMemory_unmapped=false;
uint16_t dos_memseg=0xc900;
uint8_t country_info[0x22]={},country_info_pc98[0x22]={};
'''+text[text.index('static void DOS_TableBindings'):]+r'''
std::string save(){std::ostringstream s;POD_Save_DOS_Tables(s);assert(s);return s.str();}
bool load(const std::string &b){std::istringstream s(b);POD_Load_DOS_Tables(s);return bool(s);}
int main(){
 CallBack_Handlers[call_casemap]=DOS_CaseMapFunc;
 const auto code=CALLBACK_PhysPointer(call_casemap);
 ram[code]=0xfe;ram[code+1]=0x38;ram[code+2]=call_casemap;ram[code+4]=0xcb;
 dos.tables.country=country_info;country_info[0]=2;country_info[0x12]=42;
 const auto bytes=save();country_info[0]=0;assert(load(bytes));assert(country_info[0]==2);
 assert(dos.tables.country==country_info);
 for(size_t n=0;n<bytes.size();++n)assert(!load(bytes.substr(0,n)));
 // Every recorded binding mismatches independently without changing the owner.
 for(size_t n=0;n<24;++n){auto b=bytes;b[n*4]^=1;assert(!load(b));}
 // Allocation count/role/location/span all belong to the original allocator.
 for(size_t n=24*4;n<bytes.size()-sizeof(country_info)-sizeof(dos_memseg);++n){
   auto b=bytes;b[n]^=1;assert(!load(b));
 }
 assert(DOS_TableUpCase==9 && dos.tables.mediaid==1 && call_casemap==12);
 auto bad=bytes;bad[bad.size()-sizeof(dos_memseg)-sizeof(country_info)+0x12]^=1;assert(!load(bad));
 dos_memseg=0xd001;assert(!load(save()));dos_memseg=0xc7ff;assert(!load(save()));
 dos_memseg=0xc901;assert(load(bytes) && dos_memseg==0xc900);
 assert(load(bytes));
 for(unsigned n=0;n<5;++n){ram[code+n]^=1;assert(!load(bytes));
   std::ostringstream refused;POD_Save_DOS_Tables(refused);assert(!refused);
   ram[code+n]^=1;assert(load(bytes));}
 CallBack_Handlers[call_casemap]=wrong_handler;assert(!load(bytes));
 {std::ostringstream s;POD_Save_DOS_Tables(s);assert(!s);}
 CallBack_Handlers[call_casemap]=DOS_CaseMapFunc;assert(load(bytes));
 const auto slot=call_casemap;call_casemap=CB_MAX;assert(!load(bytes));
 {std::ostringstream s;POD_Save_DOS_Tables(s);assert(!s);}
 call_casemap=slot;assert(load(bytes));
 DOS_GetMemory_unmapped=true;assert(!load(bytes));
}
'''
path=work/'consumer.cpp';path.write_text(source)
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(path),'-o',str(work/'consumer')],check=True)
subprocess.run([str(work/'consumer')],check=True)
print('DOS tables: all binding mismatches, truncations, country/casemap and private cursor bounds passed')

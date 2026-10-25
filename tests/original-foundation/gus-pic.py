#!/usr/bin/env python3
"""Focused native callback relocation/forwarding regression, not audio qualification."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
gus = (root/'src/hardware/gus.cpp').read_text()
pic = (root/'src/hardware/pic.cpp').read_text()
sb = (root/'src/hardware/sblaster.cpp').read_text()
def function(src, signature):
    start = src.index(signature)
    brace = src.index('{', start)
    depth = 1
    end = brace+1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end]
callback = function(gus, 'static void GUS_MidiTxDone(Bitu val)')
table = pic[pic.index('const void *pic_state_event_table[]'):]
table = table[:table.index('};')+2]
# Use actual callback table order, with inert stand-ins for other device callbacks.
entries = re.findall(r'^\s*([A-Za-z_][A-Za-z_0-9]*),', table, re.M)
assert entries.count('GUS_MidiTxDone_PIC_Event') == 1
standins = ''.join('void *'+name+' = (void*)&inert;\n' for name in dict.fromkeys(entries) if name not in ('NULL', 'GUS_MidiTxDone_PIC_Event'))
timer_table = pic[pic.index('const void *pic_state_timer_table[]'):]
timer_table = timer_table[:timer_table.index('};')+2]
timer_entries = re.findall(r'^\s*([A-Za-z_][A-Za-z_0-9]*),', timer_table, re.M)
standins += ''.join('void *'+name+' = (void*)&inert;\n' for name in dict.fromkeys(timer_entries) if name != 'NULL' and name not in entries)
export = re.search(r'^void \*GUS_MidiTxDone_PIC_Event = .*;', gus, re.M).group()
# Exact ingress case consumed by GUS write handler, including its delay and value.
start = gus.index('\t\tcase 0x301:', gus.index('PIC_RemoveEvents(GUS_MidiTxDone)'))
writecase = gus[start:gus.index('\t\tcase 0x302:', start)]
forward = function(sb, 'static void adlib_gusforward(')
read = function(sb, 'static Bitu adlib_gusread(')
serializer = pic[pic.index('class SerializePic :'):]
serializer = serializer[:serializer.index('} dummy;')+len('} dummy;')]
serializer_fixture = r'''
#include <sstream>
struct SerializeGlobalPOD {
 static SerializeGlobalPOD *component;
 explicit SerializeGlobalPOD(const char*) {component=this;}
 virtual void getBytes(std::ostream&) {}
 virtual void setBytes(std::istream&) {}
};
SerializeGlobalPOD *SerializeGlobalPOD::component=nullptr;
using pic_tickindex_t=long double;
using PIC_EventHandler=void (*)(Bitu);
using TIMER_TickHandler=void (*)();
constexpr int PIC_QUEUESIZE=8;
struct PICEntry {pic_tickindex_t index; Bitu value; PIC_EventHandler pic_event; PICEntry *next;};
struct {PICEntry entries[PIC_QUEUESIZE]={};PICEntry *free_entry=nullptr,*next_entry=nullptr;} pic_queue;
struct TickerBlock {TIMER_TickHandler handler;TickerBlock *next;};
TickerBlock *firstticker=nullptr;
unsigned long PIC_Ticks=123;
Bitu PIC_IRQCheck=0,PIC_IRQCheckPending=0;
struct {unsigned mask=0;} pics[2];
bool InEventService=false;
pic_tickindex_t srv_lag=0;
#define IS_PC98_ARCH false
void PC98_Mouse_RestoreTick() {}
'''
code = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
void E_Exit(const char *message) {throw std::runtime_error(message);}
using Bitu=uintptr_t;
void inert() {}
struct {unsigned midiControl=0,midiStatus=2,mixControl=0,midiData=0;} myGUS;
unsigned irq=0, output=0; bool available=false;
void GUS_MidiCheckIRQ(){++irq;}
bool MIDI_Available(){return available;}
void MIDI_RawOutByte(uint8_t v){output=v;}
void (*event)(Bitu)=nullptr; Bitu value=0; double delay=0;
void PIC_AddEvent(void (*cb)(Bitu),double d,Bitu v){event=cb;delay=d;value=v;}
Bitu actual_port=0, actual_value=0;
void GUS_AdLibWrite(Bitu p,uint8_t v){actual_port=p;actual_value=v;}
uint8_t GUS_AdLibRead(){return 0xa5;}
'''+callback+'\n'+export+'\n'+standins+table+'\n'+timer_table+'\n'+function(pic, 'Bitu PIC_State_IndexTimer(')+'\n'+function(pic, 'uint16_t PIC_State_FindEvent(')+'\n'+function(pic, 'Bitu PIC_State_IndexEvent(')+'\n'+function(pic, 'uint16_t PIC_State_FindTimer(')+'\n'+serializer_fixture+serializer+'\n'+forward+'\n'+read+'\nvoid write(Bitu val) {switch(0x301) {\n'+writecase+'\n}}\n'+r'''
int main(){
 write(0x91); assert(delay==0.32 && value==0x91 && !(myGUS.midiStatus&2));
 auto index=PIC_State_FindEvent((Bitu)event); assert(index!=0xffff && index!=0);
 auto restored=(void (*)(Bitu))PIC_State_IndexEvent(index); assert(restored==event);
 assert(PIC_State_IndexEvent(0xffff)==0);
 bool rejected=false;
 try {PIC_State_IndexEvent(sizeof(pic_state_event_table)/sizeof(pic_state_event_table[0]));}
 catch(const std::runtime_error&) {rejected=true;}
 assert(rejected);
 assert(PIC_State_IndexTimer(0xffff)==0);
 rejected=false;
 try {PIC_State_IndexTimer(sizeof(pic_state_timer_table)/sizeof(pic_state_timer_table[0]));}
 catch(const std::runtime_error&) {rejected=true;}
 assert(rejected);
 myGUS.mixControl=0x20; restored(value);
 assert(myGUS.midiData==0x91 && (myGUS.midiStatus&3)==3);
 myGUS.mixControl=0; available=true; restored(0x7e); assert(output==0x7e);
 for (auto port : {0x388,0x389,0x220}) {
  adlib_gusforward(port,0x179,1); assert(actual_port==(Bitu)port && actual_value==0x79);
 }
 assert(adlib_gusread(0x388,1)==0xa5);
 // Exercise actual original-owner serialization and queue reconstruction,
 // including retained event value/time and ticker order (not a live mixer).
 pic_queue.entries[0]={0.32L,0x91,event,nullptr};
 pic_queue.next_entry=&pic_queue.entries[0];
 pic_queue.free_entry=&pic_queue.entries[1];
 for(unsigned i=1;i<PIC_QUEUESIZE;i++)
  pic_queue.entries[i].next=i+1<PIC_QUEUESIZE?&pic_queue.entries[i+1]:nullptr;
 firstticker=new TickerBlock{(TIMER_TickHandler)pic_state_timer_table[1],
  new TickerBlock{(TIMER_TickHandler)pic_state_timer_table[3],nullptr}};
 std::stringstream archive;
 SerializeGlobalPOD::component->getBytes(archive);
 pic_queue.next_entry=nullptr; PIC_Ticks=999;
 const std::string saved=archive.str();
 SerializeGlobalPOD::component->setBytes(archive);
 assert(PIC_Ticks==123 && pic_queue.next_entry==&pic_queue.entries[0]);
 assert(pic_queue.next_entry->pic_event==event && pic_queue.next_entry->value==0x91);
 assert((double)pic_queue.next_entry->index==0.32);
 assert(pic_queue.free_entry==&pic_queue.entries[1]);
 assert(firstticker && firstticker->next && !firstticker->next->next);
 while(firstticker){auto next=firstticker->next;delete firstticker;firstticker=next;}
 // Queue link corruption/truncation must stop at the fatal install boundary.
 // Base POD fixture is empty; offset follows actual serialized fields above.
 const size_t first_link=sizeof(PIC_Ticks)+sizeof(PIC_IRQCheck)+sizeof(PIC_IRQCheckPending)+
  sizeof(pics)+sizeof(double)+sizeof(Bitu)+sizeof(uint16_t);
 std::string corrupt=saved; uint16_t bad=PIC_QUEUESIZE;
 corrupt.replace(first_link,sizeof(bad),reinterpret_cast<const char*>(&bad),sizeof(bad));
 rejected=false;
 try {std::stringstream input(corrupt);SerializeGlobalPOD::component->setBytes(input);}
 catch(const std::runtime_error&) {rejected=true;}
 assert(rejected);
 rejected=false;
 try {std::stringstream input(saved.substr(0,first_link));SerializeGlobalPOD::component->setBytes(input);}
 catch(const std::runtime_error&) {rejected=true;}
 assert(rejected);
}
'''
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'gus-pic-regression'
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as tmp:
    path=Path(tmp)
    (path/'test.cpp').write_text('#include <initializer_list>\n'+code)
    subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS actual GUS Tx ingress delay/value, PIC callback index roundtrip, loopback/output, SB actual-port forwarding')

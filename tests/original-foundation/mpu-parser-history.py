#!/usr/bin/env python3
"""Actual MPU direct-message parser resume; MIDI output is an explicit byte sink."""
import pathlib,os,subprocess
r=pathlib.Path(__file__).resolve().parents[2];s=(r/'src/hardware/mpu401.cpp').read_text()
layout=s[s.index('enum MpuMode'):s.index('int MPU401_GetIRQ')]
parser=s[s.index('static void MPU401_WriteData(Bitu port,Bitu val,Bitu iolen) {'):s.index('static void MPU401_IntelligentOut')]
codec=s[s.index('void POD_Save_MPU401'):]
code=r'''#include <sstream>
#include <vector>
#include <cstring>
#include <cassert>
#include <cstdint>
using Bitu=uintptr_t;using Bits=intptr_t;
#define MPU401_QUEUE 32
#define MSG_EOX 0xf7
#define MSG_MPU_ACK 0xfe
#define LOG_MISC 0
#define LOG_ERROR 0
struct Diagnostic{void operator()(const char*,...) {}};
#define LOG(a,b) Diagnostic()
#define WRITE_POD(p,v) stream.write(reinterpret_cast<const char*>(p),sizeof(v))
#define READ_POD(p,v) stream.read(reinterpret_cast<char*>(p),sizeof(v))
std::vector<uint8_t> bytes;
void MIDI_RawOutByte(uint8_t b){bytes.push_back(b);}
void MPU401_EOIHandlerDispatch(){}
bool test=true;
'''+layout+parser+codec+r'''
int main(){
 mpu.mode=M_INTELLIGENT;mpu.state.wsd=true;mpu.state.wsd_start=true;
 MPU401_WriteData(0,0x90,1);assert(bytes==std::vector<uint8_t>{0x90});
 std::ostringstream saved;POD_Save_MPU401(saved);assert(saved);
 MPU401_WriteData(0,60,1);MPU401_WriteData(0,100,1);
 auto expected=bytes;assert((expected==std::vector<uint8_t>{0x90,60,100}));
 mpu={};bytes={0x90};std::istringstream load(saved.str());POD_Load_MPU401(load);assert(load&&bytes.size()==1);
 MPU401_WriteData(0,60,1);assert(mpu.state.wsd);MPU401_WriteData(0,100,1);assert(!mpu.state.wsd&&bytes==expected);
 mpu={};bytes.clear();mpu.mode=M_INTELLIGENT;mpu.state.wsm=true;mpu.state.wsd_start=true;
 MPU401_WriteData(0,0xf2,1);MPU401_WriteData(0,3,1);
 std::ostringstream system;POD_Save_MPU401(system);mpu={};bytes.clear();
 std::istringstream resume(system.str());POD_Load_MPU401(resume);assert(resume&&bytes.empty());
 MPU401_WriteData(0,4,1);assert(!mpu.state.wsm&&(bytes==std::vector<uint8_t>{4}));
}
'''
w=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'mpu-parser-test';w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(code)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined','-g',str(w/'test.cpp'),'-o',str(w/'test')],check=True)
subprocess.run([str(w/'test')],check=True)
print('actual MPU direct channel/system parser mid-message continuation without import output passed')

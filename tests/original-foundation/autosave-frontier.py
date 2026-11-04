#!/usr/bin/env python3
"""Actual autosave entry refuses before focus/UI/native dispatch (reduced UI).
No source-shape assertion: execute both owning entry points with denied frontier.
"""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
w=Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'autosave-frontier';w.mkdir(exist_ok=True)
t=(root/'src/misc/savestates.cpp').read_text()
entry=t[t.index('\tvoid SaveGameState(bool pressed)'):t.index('\n\n\tvoid LoadGameState',t.index('\tvoid SaveGameState(bool pressed)'))]
slot=t[t.index('void SaveState::save(size_t slot)'):t.index('bool SaveState::saveFile(',t.index('void SaveState::save(size_t slot)'))]
code=r'''
#include <string>
#include <cassert>
#include <cstddef>
bool admitted=false;unsigned focus=0,dialogs=0,saves=0;
bool save_state_frontier(){return admitted;}
void LOG_MSG(const char*){}
void GFX_LosingFocus(){++focus;}
void MAPPER_ReleaseAllKeys(){++dialogs;}
void GFX_ReleaseMouse(){++dialogs;}
bool GFX_IsFullscreen(){return false;}
void GFX_SwitchFullScreen(){++dialogs;}
const char*tinyfd_inputBox(const char*,const char*,const char*){++dialogs;return "remark";}
bool auto_save_state=true,noremark_save_state=false;
unsigned currentSlot=0,page=0;
unsigned GetGameState(){return 0;}
void SetGameState(int){}
void refresh_slots(){}
void notifyError(const std::string&){}
struct SaveState {
 using Error=std::string;
 static constexpr unsigned SLOT_COUNT=10;
 static SaveState&instance(){static SaveState s;return s;}
 void save(size_t);
 bool saveTo(size_t,void*,void*,void*,const std::string&){++saves;return true;}
};
bool DEBUG_NativeStateSlot(size_t,bool,const std::string&){++saves;return true;}
'''+entry+'\n'+slot+r'''
int main(){
 for(bool automatic:{false,true}){
  auto_save_state=automatic;admitted=false;
  SaveGameState(false);SaveGameState(true);SaveState::instance().save(0);
  assert(!focus && !dialogs && !saves);
 }
 admitted=true;SaveGameState(true);assert(focus==1 && saves==1);
}
'''
(w/'test.cpp').write_text(code)
for debug in (0,1):
 exe=w/('test-'+str(debug))
 subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=undefined',f'-DC_DEBUG={debug}',str(w/'test.cpp'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Actual slot/autosave entry: denied frontier has no focus/key/UI/save effects, debug and no-debug; reduced UI doubles')

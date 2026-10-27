/* GPL-2.0-or-later. Data-only mouse diagnostic snapshot, emulator thread. */
#ifndef DOSBOX_MOUSE_DIAGNOSTIC_H
#define DOSBOX_MOUSE_DIAGNOSTIC_H
#include <stdint.h>
#include <string.h>
// Fixed recent window, oldest first; overflow explicitly observable. No allocation.
enum class MouseWitnessRoute : uint8_t { Motion=1, Position=2, RangeX=3, RangeY=4, Poll=5, Interrupt=6, ModeBefore=7, ModeAfter=8 };
struct MouseWitnessEvent {
    uint64_t sequence=0;uint32_t tick=0;
    uint16_t mode=0,operation=0,callback_cs=0,callback_ip=0,return_cs=0,return_ip=0;
    bool return_valid=false;
    MouseWitnessRoute route=MouseWitnessRoute::Motion;
    float x=0,y=0;int screen_x=0,screen_y=0,min_x=0,max_x=0,min_y=0,max_y=0;
    uint16_t gran_x=0,gran_y=0,ax=0,bx=0,cx=0,dx=0,dest_seg=0,dest_off=0;
};
struct MouseWitnessWindow {
    MouseWitnessEvent events[16];unsigned count=0;
    uint64_t sequence=0,overwritten=0;bool exhausted=false;
    void append(MouseWitnessEvent e) {
        if(exhausted)return;
        if(sequence==UINT64_MAX){exhausted=true;return;}
        e.sequence=++sequence;
        if(count==16){for(unsigned i=1;i<16;i++)events[i-1]=events[i];--count;++overwritten;}
        events[count++]=e;
    }
};
// Explicit first-after-action diagnostic; reset only by successful arm command.
struct MouseFirstWitness {
    char action_id[65]={};unsigned count=0;
    MouseWitnessEvent events[4];
    bool armed=false,matched=false,frozen=false,overflow=false,exhausted=false;
    uint64_t generation=0;
    bool arm(const char *id) {
        if(generation==UINT64_MAX)return false;
        const uint64_t next=generation+1;
        *this=MouseFirstWitness();generation=next;strcpy(action_id,id);armed=true;return true;
    }
    void record(const MouseWitnessEvent &e,const char *submitted) {
        if(!armed || frozen)return;
        if(!matched) {
            if(e.route!=MouseWitnessRoute::Motion || !submitted || strcmp(submitted,action_id))return;
            matched=true;
        }
        if(count==4){overflow=true;frozen=true;return;}
        events[count++]=e;
        if(e.route==MouseWitnessRoute::Position || e.route==MouseWitnessRoute::Poll || e.route==MouseWitnessRoute::Interrupt)frozen=true;
    }
};
struct MouseDiagnostic {
    MouseWitnessWindow recent;
    MouseFirstWitness first;
    MouseWitnessEvent motion_end;
    float internal_x=0,internal_y=0,absolute_x=0,absolute_y=0;
    uint32_t motion_tick=0;
    int screen_x=0,screen_y=0,min_x=0,max_x=0,min_y=0,max_y=0;
    uint16_t returned_x=0,returned_y=0;
    uint64_t return_sequence=0;
    uint32_t return_tick=0;
};
MouseDiagnostic Mouse_GetDiagnostic();
void Mouse_CheckpointInvalidateObservations();
bool Mouse_ArmFirstWitness(const char *id);
void Mouse_SubmittedMotion(const char *id);
#endif

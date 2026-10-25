#include "native_stop_conditions.h"
#include <cassert>
#include <thread>
namespace {
unsigned calls=0;
bool verdict=false, violation=false;
bool held(bool &error) {
    ++calls;std::this_thread::sleep_for(std::chrono::milliseconds(30));
    error=violation;return verdict;
}
}
int main() {
    using namespace NativeDebug;
    StopCondition text;text.kind=StopCondition::Kind::Text;
    text.needle.assign("A\0B",3);text.region={2,3,4,2};
    assert(!validateConditions({text}));
    TextSnapshot sample;sample.region=text.region;sample.cells.resize(16,' ');
    sample.cells[2]='A';sample.cells[4]=0;sample.cells[6]='B';
    unsigned x=0,y=0;assert(matchText(text,sample,x,y) && x==3 && y==3);
    sample.cells[6]='C';assert(!matchText(text,sample,x,y));
    assert(validateConditions({text,text}));
    StopCondition guest;guest.kind=StopCondition::Kind::GuestElapsed;
    guest.ms=86400000;assert(!validateConditions({guest}));
    guest.ms++;assert(validateConditions({guest}));
    StopCondition address;assert(validateConditions({address,address}));
    text.region.x=80;assert(validateConditions({text}));
    const StopObservations observe{
        [](const StopCondition &){return true;},
        [](const TextRegion &,TextSnapshot &){return false;},
        [](bool &error){error=true;return false;}
    };
    guest.ms=5;
    StopRuntime runtime;runtime.begin(2,10);
    assert(!evaluateConditions({guest},runtime,14,true,observe));
    assert(evaluateConditions({guest},runtime,15,true,observe));
    assert(runtime.evidence.condition_index==1 && runtime.evidence.end_tick==15);
    runtime.begin(1,UINT32_MAX-1);
    assert(evaluateConditions({guest},runtime,0,true,observe));
    assert(runtime.evidence.error=="clock_discontinuity");
    runtime.begin(1,100);
    assert(!evaluateConditions({guest},runtime,104,true,observe));
    assert(evaluateConditions({guest},runtime,105,true,observe));
    runtime.begin(2,10);guest.ms=0;
    assert(evaluateConditions({guest,address},runtime,10,false,observe));
    assert(runtime.evidence.condition_index==1 && !runtime.evidence.guest_advanced);
    runtime.begin(2,10);
    assert(evaluateConditions({address,guest},runtime,10,false,observe));
    assert(runtime.evidence.condition_index==1);
    text.region={0,0,80,25};runtime.begin(2,10);
    assert(evaluateConditions({text,guest},runtime,10,false,observe));
    assert(runtime.evidence.condition_index==1 && runtime.evidence.error=="condition_unavailable");
    StopCondition host;host.kind=StopCondition::Kind::HostElapsed;host.ms=10;
    StopCondition predicate;predicate.kind=StopCondition::Kind::Predicate;
    const StopObservations heldObserve{observe.address,observe.text,held};
    for(bool truth:{false,true}) {
        verdict=truth;violation=false;calls=0;runtime.begin(2,100);
        assert(evaluateConditions({host,predicate},runtime,100,false,heldObserve));
        assert(calls==1 && runtime.evidence.condition_index==1);
        assert(runtime.evidence.host_elapsed_ms>=10 && runtime.evidence.error.empty());
    }
    calls=0;verdict=false;runtime.begin(2,100);
    assert(evaluateConditions({predicate,host},runtime,100,false,heldObserve));
    assert(calls==1 && runtime.evidence.condition_index==2);
    calls=0;violation=true;runtime.begin(2,100);
    assert(evaluateConditions({host,predicate},runtime,100,false,heldObserve));
    assert(calls==1 && runtime.evidence.error=="observer_error");
    violation=false;calls=0;runtime.begin(2,100);runtime.due[1]=101;
    assert(!evaluateConditions({host,predicate},runtime,100,false,heldObserve));
    assert(evaluateConditions({host,predicate},runtime,101,true,heldObserve));
    assert(calls==1 && runtime.evidence.condition_index==1 && runtime.evidence.guest_advanced);
}

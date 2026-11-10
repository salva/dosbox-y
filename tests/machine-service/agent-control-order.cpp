// Exercise the real Agent queue with controlled native state predicates.
#include "agent/agent_bridge.h"
#include <cassert>
#include <vector>
static bool pending=false,active=false;
bool DEBUG_NativeExecutionPending() { return pending; }
bool DEBUG_NativeExecutionInFlight() { return active || pending; }
int main() {
    dosbox_agent::EmulationThreadQueue queue;
    assert(queue.BindToCurrentThread());
    std::vector<int> effects;
    queue.Submit([&](uint64_t){effects.push_back(1);});
    queue.SubmitStop([&](uint64_t){effects.push_back(2);});
    queue.Submit([&](uint64_t){effects.push_back(3);});
    pending=true;
    assert(queue.Pump()==0 && effects.empty());
    pending=false;active=true;
    assert(queue.Pump()==1 && effects==std::vector<int>{2});
    assert(queue.Pump()==0); // ordinary effects still deferred
    active=false;
    assert(queue.Pump()==2 && effects==std::vector<int>({2,1,3}));
    std::string error;
    assert(dosbox_agent::AGENT_RunQueueSelfTest(&error));
}

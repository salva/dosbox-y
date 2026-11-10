#include "native_debug_control.h"
#include "native_frontier.h"
#include <cassert>
#include <chrono>
#include <cstdio>
using namespace NativeDebug;
struct Fixture {
    Execution execution;
    ControlMailbox mailbox;
    Attachment attachment;
    unsigned stops=0, turns=0;
    static void held(void *opaque,const NativeFrontier::Descriptor &) {
        auto &f=*static_cast<Fixture *>(opaque);
        const auto op=f.execution.operation();
        assert(f.execution.state()==State::Pending);
        Control request;request.kind=Control::Kind::Pause;
        request.attachment=f.attachment;request.operation=op;
        assert(f.mailbox.submit(request));
        f.service();
        Status reply;assert(f.mailbox.take(reply));
        assert(reply.result==ControlResult::Pending && !f.stops);
        assert(f.execution.state()==State::Pending && !f.execution.decoderCalls());
        // The held owner, not control consumption or an outer loop, activates.
        ++f.turns;Request active;assert(f.execution.activate(7,active));
    }
    void service() {
        mailbox.service([this](Control request) {
            auto status=request.kind==Control::Kind::Pause?
                execution.pause(request.attachment,request.operation):
                execution.observe(request.attachment,request.operation);
            if(status.result==ControlResult::StopRequested)++stops;
            return status;
        });
    }
};
int main() {
    Fixture f;f.attachment=f.execution.attach(Frontend::Lua);
    auto context=f.execution.context(7);
    Request request;request.frontend=Frontend::Lua;request.attachment=f.attachment;
    request.expected=context;request.resume=true;request.retain_result=true;
    request.completion=Request::Completion::NativeBoundary;
    assert(f.execution.enqueue(request,context,true)==Admission::Accepted);
    const auto first=f.execution.operation();
    NativeFrontier::Frontiers frontier;
    assert(frontier.bind(Fixture::held,&f));
    frontier.reach(NativeFrontier::Kind::Predispatch,7);
    assert(f.turns==1 && f.execution.state()==State::Active);
    Control pause;pause.kind=Control::Kind::Pause;pause.attachment=f.attachment;pause.operation=first;
    assert(f.mailbox.submit(pause));assert(!f.mailbox.submit(pause));
    f.service();Status reply;assert(f.mailbox.take(reply));
    assert(reply.result==ControlResult::StopRequested && reply.state==State::Active && f.stops==1);
    assert(f.execution.state()==State::Active); // acceptance is not settlement
    f.execution.settle(0,false);
    assert(f.execution.pause(f.attachment,first).result==ControlResult::Terminal);
    assert(f.execution.release(f.attachment,first));
    assert(f.execution.observe(f.attachment,first).result==ControlResult::Unknown);
    context=f.execution.context(7);request.expected=context;
    assert(f.execution.enqueue(request,context,true)==Admission::Accepted);
    Request active;assert(f.execution.activate(7,active));
    assert(f.mailbox.submit(pause));f.service();assert(f.mailbox.take(reply));
    assert(reply.result==ControlResult::Unknown && f.stops==1);
    const auto second=f.execution.operation();
    assert(f.execution.observe({f.attachment.serial+1},second).result==ControlResult::Unknown);
    assert(f.execution.close(f.attachment));
    assert(f.execution.pause(f.attachment,second).result==ControlResult::Departed);
    constexpr unsigned iterations=10000000;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<iterations;++i)f.service();
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::printf("control: Pending held barrier, Active exact stop, stale successor, departure passed; %u empty services in %lld us (fixture only; no latency bound)\n",iterations,(long long)elapsed);
}

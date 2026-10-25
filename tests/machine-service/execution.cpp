#include "native_debug_execution.h"
#include <cassert>
using namespace NativeDebug;
int main() {
    Execution slot;
    const auto attachment=slot.attach(Frontend::Interactive,1);
    assert(attachment.serial);
    assert(!slot.attach(Frontend::Agent,1).serial);
    assert(!slot.attach(Frontend::Interactive,2).serial);
    auto context=slot.context(7);
    Request request;request.attachment=attachment;request.expected=context;
    assert(slot.enqueue(request,context,false)==Admission::UnsafeContext);
    auto stale=request;stale.expected.checkpoint++;
    assert(slot.enqueue(stale,context,true)==Admission::StaleContext);
    assert(slot.enqueue(request,context,true)==Admission::Accepted);
    assert(slot.state()==State::Pending && !slot.context(7).invocation);
    assert(slot.enqueue(request,context,true)==Admission::Busy);
    Request active;
    assert(!slot.activate(8,active));
    assert(slot.activate(7,active));
    const auto operation=slot.operation();
    slot.startBudget(0,0);slot.decoderEntered();
    assert(slot.close(attachment));
    assert(!slot.detach(attachment));
    assert(!slot.attach(Frontend::Agent).serial);
    slot.settle(1,false);
    const auto *result=slot.result(attachment);
    assert(result && result->terminal==Terminal::AttachmentLost && result->failed);
    assert(result->operation==operation && result->start.checkpoint==context.checkpoint);
    assert(!slot.release(attachment,operation+1));
    assert(slot.result(attachment)==result);
    assert(slot.release(attachment,operation));
    assert(slot.detach(attachment));
    const auto agent=slot.attach(Frontend::Agent,2);
    assert(agent.serial && agent.serial!=attachment.serial);
    assert(!slot.admits(attachment));
    context=slot.context(8);request={};request.frontend=Frontend::Agent;
    request.attachment=agent;request.expected=context;request.resume=true;
    assert(slot.enqueue(request,context,true)==Admission::Accepted);
    assert(slot.activate(8,active));slot.startBudget(0,0);
    assert(!slot.budgetExpired(1000000,1000000)); // Public RUN is unlimited.
    assert(slot.ownerDeparting(8));
    result=slot.result(agent);
    assert(result && result->terminal==Terminal::OwnerLost);
    assert(slot.release(agent,result->operation));
    assert(!slot.checkpointChanged(context));
    context=slot.context(9);request.expected=context;request.resume=false;
    assert(slot.enqueue(request,context,true)==Admission::Accepted);
    assert(slot.activate(9,active));slot.startBudget(0,0);
    assert(slot.budgetExpired(1001,0));
    slot.settle(0,false);result=slot.result(agent);
    assert(result && result->terminal==Terminal::Budget && result->budget_exhausted);
    const auto end=result->end;
    assert(slot.release(agent,result->operation));
    assert(slot.checkpointChanged(end));
    assert(!slot.checkpointChanged(end));
}

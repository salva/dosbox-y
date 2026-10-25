#include "lua_driver_worker.h"
#include <cassert>
#include <thread>
using namespace LuaDriver;
int main() {
    Options options;options.select("lua-driver-code",
        "local d=require('dosbox'); local c=d.context(); "
        "local x=41; assert(d.registers(c).registers.ax==42); "
        "package.preload.m=function() return d.step() end; "
        "assert(require('m').boundary=='decoder'); "
        "assert(x+1==42); assert(not pcall(d.read,c,0,0)); assert(d.read(c,0,1)=='*'); "
        "assert(d.run_until(c,1,2).boundary=='decoder'); "
        "local s=d.save('/tmp/witness',30000); assert(s.boundary=='decoder'); "
        "assert(s.effects.published and not s.effects.completed and not s.effects.retired); "
        "assert(s.effects.cleanup_failed and not s.effects.durable and s.effects.archive_bytes==17); "
        "assert(s.effects.code=='cleanup_failed' and s.effects.archive_sha256=='fixture'); "
        "assert(s.error=='native cleanup failed'); "
        "setmetatable({}, {__gc=function() assert(not pcall(d.step)) end})");
    Handoff handoff;WorkerResult result;
    std::thread worker([&]{runWorker(options,handoff,result);});
    assert(handoff.ready());unsigned calls=0;
    while(!handoff.isLuaClosed()) {
        Request request;
        if(handoff.take(request)) {
            if(request.operation==Operation::RunUntil)assert(request.cs==1 && request.ip==2);
            if(request.operation==Operation::Save)assert(request.path=="/tmp/witness" && request.timeout_ms==30000);
            Response response;response.context={7,1};response.registers_valid=true;
            response.ax=42;response.boundary="decoder";response.bytes={42};
            if(request.operation==Operation::Save) {
                response.save.valid=true;response.save.published=true;
                response.save.cleanup_attempted=true;response.save.cleanup_failed=true;
                response.save.archive_bytes=17;response.save.archive_sha256="fixture";
                response.save.code="cleanup_failed";response.error="native cleanup failed";
            }
            assert(handoff.complete(response));++calls;
        } else std::this_thread::yield();
    }
    worker.join();assert(result.completed && result.error.empty() && calls==6);
    // Real Lua protected observer, direct-read subrequest and caught effect
    // violation. Native authority here is deliberately a transport fixture.
    Options observing;observing.select("lua-driver-code",
        "local d=require('dosbox'); local seen=0; local alias=d.save; "
        "assert(not pcall(d.watchpoint,1,4294967296)); "
        "assert(not pcall(d.watchpoint,-1,0)); "
        "assert(not pcall(d.watchpoint,1,0,false)); "
        "local b=d.watchpoint(1,65538,function(e) seen=seen+1; "
        "assert(e.registers.ax==42 and d.read(e.context,0,1)=='*'); "
        "assert(not pcall(alias,'unused')); assert(not pcall(d.read,e.context,-1,1)); "
        "assert(not pcall(d.read,e.context,0,0)); "
        "return false end); d.run(); assert(seen==1); d.remove(b.registration)");
    Handoff events;WorkerResult observed;assert(events.ready());
    std::thread observing_worker([&]{runWorker(observing,events,observed);});
    uint64_t registration=0;bool verdict_seen=false;unsigned direct_reads=0;
    while(!events.isLuaClosed()) {
        Request request;
        if(events.take(request)) {
            Response response;
            if(request.operation==Operation::Watchpoint) {
                registration=request.registration;
                assert(registration && request.cs==1 && request.address==65538 && request.observer);
                response.registration=registration;assert(events.complete(response));
            } else if(request.operation==Operation::Run) {
                Event event;event.ordinal=1;event.registration=registration;
                event.image.context={7,2};event.image.registers_valid=true;event.image.ax=42;
                assert(events.offerEvent(event));
            } else {
                assert(request.operation==Operation::Remove && request.registration==registration);
                assert(events.complete(response));
            }
        }
        if(events.takeObserverRead(request)) {
            assert(request.context.invocation==7 && request.context.checkpoint==2);
            Response read;read.bytes={42};assert(events.completeObserverRead(read));++direct_reads;
        }
        Verdict verdict;
        if(events.takeVerdict(verdict)) {
            assert(verdict.propagate && verdict.violation && !verdict.error);
            verdict_seen=true;Response response;assert(events.complete(response));
        }
        std::this_thread::yield();
    }
    observing_worker.join();assert(observed.completed && verdict_seen && direct_reads==1);
    Options verdict_options;verdict_options.select("lua-driver-code",
        "local d=require('dosbox'); local n=0; d.breakpoint(1,2,function(e) "
        "n=n+1; if n==1 then return false elseif n==2 then error('observer error') "
        "elseif n==3 then coroutine.yield() else return 'not a verdict' end end); "
        "for i=1,4 do d.run() end; assert(n==4)");
    Handoff verdict_events;WorkerResult verdict_result;assert(verdict_events.ready());
    std::thread verdict_worker([&]{runWorker(verdict_options,verdict_events,verdict_result);});
    unsigned ordinal=0,verdicts=0;
    while(!verdict_events.isLuaClosed()) {
        Request request;
        if(verdict_events.take(request)) {
            if(request.operation==Operation::Breakpoint) {
                Response response;response.registration=request.registration;
                assert(verdict_events.complete(response));
            } else {
                assert(request.operation==Operation::Run);
                Event event;event.ordinal=++ordinal;event.registration=1;
                assert(verdict_events.offerEvent(event));
            }
        }
        Verdict verdict;
        if(verdict_events.takeVerdict(verdict)) {
            ++verdicts;
            assert(verdict.violation==(verdicts==3) && verdict.error==(verdicts!=1));
            assert(verdict.propagate==(verdicts!=1));
            assert(verdict_events.complete(Response{}));
        }
        std::this_thread::yield();
    }
    verdict_worker.join();assert(verdict_result.completed && verdicts==4);
    // A caught finalizer effect is sticky during a protected observation.
    Options finalizing;finalizing.select("lua-driver-code",
        "local d=require('dosbox'); local finalized=false; "
        "d.breakpoint(1,2,function(e) "
        "do local x=setmetatable({}, {__gc=function() "
        "finalized=true; assert(not pcall(d.step)) end}) end; "
        "collectgarbage('collect'); assert(finalized); return false end); d.run()");
    Handoff finalizer_events;WorkerResult finalizer_result;assert(finalizer_events.ready());
    std::thread finalizer_worker([&]{runWorker(finalizing,finalizer_events,finalizer_result);});
    bool finalizer_verdict=false;
    while(!finalizer_events.isLuaClosed()) {
        Request request;
        if(finalizer_events.take(request)) {
            if(request.operation==Operation::Breakpoint) {
                Response response;response.registration=request.registration;
                assert(finalizer_events.complete(response));
            } else {
                assert(request.operation==Operation::Run);
                Event event;event.ordinal=1;event.registration=1;
                assert(finalizer_events.offerEvent(event));
            }
        }
        Verdict verdict;
        if(finalizer_events.takeVerdict(verdict)) {
            assert(verdict.propagate && verdict.violation && !verdict.error);
            finalizer_verdict=true;assert(finalizer_events.complete(Response{}));
        }
        std::this_thread::yield();
    }
    finalizer_worker.join();assert(finalizer_result.completed && finalizer_verdict);
    // An ordinary script hook cannot disable the predicate budget.
    Options bounded;bounded.select("lua-driver-code",
        "local d=require('dosbox'); debug.sethook(); "
        "d.run{stop_any={{kind='predicate',test=function() while true do end end}}}");
    Handoff budgets;WorkerResult bounded_result;assert(budgets.ready());
    std::thread bounded_worker([&]{runWorker(bounded,budgets,bounded_result);});
    bool budget_seen=false;
    while(!budgets.isLuaClosed()) {
        Request request;
        if(budgets.take(request)) {
            assert(request.operation==Operation::Run && request.stop_any.size()==1);
            Event event;event.ordinal=1;event.registration=0;
            assert(budgets.offerEvent(event));
        }
        Verdict verdict;
        if(budgets.takeVerdict(verdict)) {
            assert(verdict.error && verdict.violation && verdict.propagate);
            budget_seen=true;assert(budgets.complete(Response{}));
        }
        std::this_thread::yield();
    }
    bounded_worker.join();assert(bounded_result.completed && budget_seen);
    Options failure;failure.select("lua-driver-code","error('worker-error')");
    Handoff other;WorkerResult error;assert(other.ready());
    std::thread broken([&]{runWorker(failure,other,error);});broken.join();
    assert(!error.completed && error.error.find("worker-error")!=std::string::npos && other.isLuaClosed());
    Options spinning;spinning.select("lua-driver-code",
        "require('dosbox').context(); while true do end");
    Handoff spin;WorkerResult spin_result;assert(spin.ready());
    std::thread spinner([&]{runWorker(spinning,spin,spin_result);});
    Request spin_request;
    while(!spin.take(spin_request))std::this_thread::yield();
    assert(spin_request.operation==Operation::Context);
    assert(spin.complete(Response{}));
    spin.close();
    while(!spin.isLuaClosed())std::this_thread::yield();
    spinner.join();
    assert(!spin_result.completed && spin_result.error=="driver closing");
    // Close is a single cooperative exception, not a recurring fault during
    // protected cleanup. Native effects remain refused after it is caught.
    Options catching;catching.select("lua-driver-code",
        "local d=require('dosbox'); local ok,err=pcall(function() "
        "d.context(); while true do end end); "
        "assert(not ok and err:find('driver closing',1,true)); "
        "local n=0; for i=1,100000 do n=n+1 end; assert(n==100000); "
        "assert(not pcall(d.step)); "
        "setmetatable({}, {__gc=function() assert(not pcall(d.step)) end})");
    Handoff caught;WorkerResult caught_result;assert(caught.ready());
    std::thread catcher([&]{runWorker(catching,caught,caught_result);});
    Request caught_request;
    while(!caught.take(caught_request))std::this_thread::yield();
    assert(caught_request.operation==Operation::Context);
    caught.close(); // Claimed request must still settle before Lua can unwind.
    assert(!caught.isLuaClosed());
    assert(caught.complete(Response{}));
    catcher.join();
    assert(caught_result.completed && caught_result.error.empty() && caught.isLuaClosed());
    Options cancelled;cancelled.select("lua-driver-code","error('must not execute')");
    Handoff closing;WorkerResult stopped;closing.close();
    std::thread early([&]{runWorker(cancelled,closing,stopped);});early.join();
    assert(!stopped.completed && stopped.error=="driver closing" && closing.isLuaClosed());
}

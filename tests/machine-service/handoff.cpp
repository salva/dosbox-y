/* GPL-2.0-or-later. Transport tests, not native seam qualification. */
#include "lua_driver_handoff.h"
#include <cassert>
#include <future>
#include <thread>
using namespace LuaDriver;
int main() {
    Handoff h;
    auto worker=std::async(std::launch::async,[&]{
        assert(h.waitReady());
        Request r;r.operation=Operation::Save;r.path="owned";
        Response s;assert(h.call(std::move(r),s));
        assert(s.reason=="published");
        assert(!h.call({},s));h.luaClosed();
    });
    assert(h.ready());
    Request r;
    while(!h.take(r))std::this_thread::yield();
    assert(r.path=="owned");
    // Close with uncertain/completed native effects never erases their result.
    h.close();Response s;s.reason="published";assert(h.complete(std::move(s)));
    worker.get();assert(h.isLuaClosed());
    Handoff before;before.close();assert(!before.waitReady());assert(!before.ready());
    Handoff pending;assert(pending.ready());
    auto canceled=std::async(std::launch::async,[&]{Response result;return pending.call({},result);});
    pending.close();assert(!canceled.get());assert(!pending.take(r));
    Handoff observed;assert(observed.ready());
    auto observing=std::async(std::launch::async,[&]{
        Response result;
        assert(observed.call({},result,[&](const Event &event){
            assert(event.ordinal==7);
            Request read;read.operation=Operation::Read;read.length=1;
            Response bytes;assert(observed.observerRead(event.ordinal,read,bytes));
            assert(bytes.bytes==std::vector<uint8_t>{42});
            // Even a caught zero-length read is an invalid event attempt.
            read.length=0;
            assert(!observed.observerRead(event.ordinal,read,bytes));
            read.length=2;read.address=UINT32_MAX;
            assert(!observed.observerRead(event.ordinal,read,bytes));
            read.length=1;read.address=0;
            // Catching/refusing an effect cannot turn this into suppression.
            read.operation=Operation::Save;
            assert(!observed.observerRead(event.ordinal,read,bytes));
            Response nested;assert(!observed.call({},nested));
            Verdict verdict;verdict.propagate=false;return verdict;
        }));
        assert(result.reason=="hit");
    });
    while(!observed.take(r))std::this_thread::yield();
    Event event;event.ordinal=7;assert(observed.offerEvent(event));
    while(!observed.takeObserverRead(r))std::this_thread::yield();
    Response bytes;bytes.bytes={42};assert(observed.completeObserverRead(bytes));
    Verdict verdict;
    while(!observed.takeVerdict(verdict))std::this_thread::yield();
    assert(verdict.propagate && verdict.violation);
    assert(!observed.takeObserverRead(r));
    Response hit;hit.reason="hit";assert(observed.complete(hit));observing.get();
    Handoff closing;assert(closing.ready());
    auto close_worker=std::async(std::launch::async,[&]{
        Response result;
        assert(closing.call({},result,[&](const Event &e){
            Request read;read.operation=Operation::Read;read.length=1;
            Response result;assert(closing.observerRead(e.ordinal,read,result));
            assert(result.bytes==std::vector<uint8_t>{99});
            Verdict v;v.propagate=false;return v;
        }));
        assert(result.reason=="closed");closing.luaClosed();
    });
    while(!closing.take(r))std::this_thread::yield();
    event.ordinal=8;assert(closing.offerEvent(event));
    while(!closing.takeObserverRead(r))std::this_thread::yield();
    closing.close();bytes.bytes={99};assert(closing.completeObserverRead(bytes));
    while(!closing.takeVerdict(verdict))std::this_thread::yield();
    assert(verdict.propagate && verdict.error);
    hit.reason="closed";assert(closing.complete(hit));close_worker.get();
    assert(closing.isLuaClosed());
    // Expired identities and caught callback failures never suppress a stop.
    Handoff expiry;assert(expiry.ready());
    auto expired_worker=std::async(std::launch::async,[&]{
        Response result;
        assert(expiry.call({},result,[&](const Event &e)->Verdict{
            Request read;read.operation=Operation::Read;read.length=1;
            Response bytes;assert(!expiry.observerRead(e.ordinal-1,read,bytes));
            throw 1;
        }));expiry.luaClosed();
    });
    while(!expiry.take(r))std::this_thread::yield();
    event.ordinal=9;assert(expiry.offerEvent(event));
    while(!expiry.takeVerdict(verdict))std::this_thread::yield();
    assert(verdict.propagate && verdict.error && verdict.violation);
    assert(expiry.complete({}));expired_worker.get();
    // Close before native verdict consumption must defeat suppression even
    // when the callback itself returns false without an error.
    Handoff late_close;assert(late_close.ready());
    std::promise<void> callback_returning;
    auto late_worker=std::async(std::launch::async,[&]{
        Response result;
        assert(late_close.call({},result,[&](const Event &)->Verdict{
            Verdict v;v.propagate=false;callback_returning.set_value();return v;
        }));
        assert(result.reason=="closed");late_close.luaClosed();
    });
    while(!late_close.take(r))std::this_thread::yield();
    event.ordinal=10;assert(late_close.offerEvent(event));
    callback_returning.get_future().wait();
    // Whichever thread acquires the handoff first after callback return,
    // consumption after close must report propagation plus the close error.
    late_close.close();
    while(!late_close.takeVerdict(verdict))std::this_thread::yield();
    assert(verdict.propagate && verdict.error);
    hit.reason="closed";assert(late_close.complete(hit));late_worker.get();
    // Host close of a claimed run forbids offering a later observer event;
    // the claimed run still needs its terminal before worker shutdown.
    Handoff offered;assert(offered.ready());
    auto offered_worker=std::async(std::launch::async,[&]{
        Response result;
        assert(offered.call({},result,[](const Event &)->Verdict{
            assert(false && "closed event must not invoke observer");return {};
        }));
        assert(result.reason=="closed");offered.luaClosed();
    });
    while(!offered.take(r))std::this_thread::yield();
    // Lock ordering is intentionally controlled by closing before offer:
    // a new event is refused and native settles the already claimed run.
    offered.close();event.ordinal=11;assert(!offered.offerEvent(event));
    hit.reason="closed";assert(offered.complete(hit));offered_worker.get();
    assert(offered.isLuaClosed());
    // Close after a native read is claimed still requires that read's
    // terminal publication. Lua must not escape its waiter or invoke another
    // subrequest, and the event verdict cannot suppress close.
    Handoff claimed_read;assert(claimed_read.ready());
    auto read_worker=std::async(std::launch::async,[&]{
        Response result;
        assert(claimed_read.call({},result,[&](const Event &e)->Verdict{
            Request read;read.operation=Operation::Read;read.length=1;
            Response image;
            assert(claimed_read.observerRead(e.ordinal,read,image));
            assert(image.bytes==std::vector<uint8_t>{17});
            assert(!claimed_read.observerRead(e.ordinal,read,image));
            Verdict v;v.propagate=false;return v;
        }));
        assert(result.reason=="closed");claimed_read.luaClosed();
    });
    while(!claimed_read.take(r))std::this_thread::yield();
    event.ordinal=12;assert(claimed_read.offerEvent(event));
    while(!claimed_read.takeObserverRead(r))std::this_thread::yield();
    claimed_read.close();
    assert(!claimed_read.isLuaClosed());
    bytes.bytes={17};assert(claimed_read.completeObserverRead(bytes));
    while(!claimed_read.takeVerdict(verdict))std::this_thread::yield();
    assert(verdict.propagate && verdict.error && verdict.violation);
    assert(!claimed_read.takeObserverRead(r));
    hit.reason="closed";assert(claimed_read.complete(hit));read_worker.get();
    assert(claimed_read.isLuaClosed());
    Request late;late.operation=Operation::Read;Response refused;
    assert(!expiry.observerRead(9,late,refused));
}

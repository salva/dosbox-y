/* GPL-2.0-or-later. Fixed nonblocking NDJSON framing and reserved replies.
 * Syntax/algebra is endpoint-owned; this primitive grants no request authority.
 */
#ifndef DOSBOX_NATIVE_TRANSPORT_H
#define DOSBOX_NATIVE_TRANSPORT_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
namespace NativeInput {
enum class TransportFault { None, EOFClosed, PartialEOF, Read, Write, Oversized,
    AssemblyTimeout, ClockUncertain, OutputTimeout };
template<size_t Capacity> struct BoundedReplyCell {
    char data[Capacity]={};
    size_t size=0,offset=0;
    bool available() const {return size==0;}
    bool set(const char *text,size_t length) {
        if(!available() || !text || length<1 || length>sizeof(data)) return false;
        memcpy(data,text,length);size=length;offset=0;return true;
    }
};
struct TransportIO {
    ssize_t (*read)(int,void *,size_t)=::read;
    ssize_t (*write)(int,const void *,size_t)=::write;
};
// Default budgets preserve the historical native regression interface. Live
// Lua uses the same framing primitive with one outstanding response and larger
// finite cells, not a second transport/parser.
template<size_t LineCapacity=2048,size_t ReplyCapacity=4096,bool SingleOutstanding=false>
class BoundedTransport {
    int input_,output_;
    char line_[LineCapacity]={};
    size_t used_=0;
    uint64_t assembly_start_=0,last_host_=0;
    bool have_host_=false,closed_=false,congested_=false;
    bool completed_line_=false,deferred_=false;
    BoundedReplyCell<ReplyCapacity> ordinary_,control_;
    bool output_waiting_=false;uint64_t output_start_=0;
    TransportFault fault_=TransportFault::None;
    void close(TransportFault fault) {closed_=true;fault_=fault;}
    bool drain(BoundedReplyCell<ReplyCapacity> &cell,size_t &budget,TransportIO io) {
        if(cell.available()) return true;
        size_t wanted=cell.size-cell.offset;
        if(wanted>budget) wanted=budget;
        if(!wanted) return false;
        ssize_t result=io.write(output_,cell.data+cell.offset,wanted);
        if(result<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return false;
        if(result<=0 || size_t(result)>wanted) {close(TransportFault::Write);return false;}
        budget-=size_t(result);cell.offset+=size_t(result);
        if(cell.offset==cell.size) {cell.size=0;cell.offset=0;return true;}
        return false;
    }
public:
    // Borrowed pipe descriptors: caller owns admission/closure/SIGPIPE handling.
    // Require nonblocking flags, never silently change shared fd flags here.
    BoundedTransport(int input,int output) : input_(input),output_(output) {
        int in=fcntl(input,F_GETFL),out=fcntl(output,F_GETFL);
        if(in<0 || out<0 || !(in&O_NONBLOCK) || !(out&O_NONBLOCK)) close(TransportFault::Read);
    }
    // Endpoint cannot reserve the required reply yet: retain this one complete
    // frame without admission/effect. Controls do not overtake unread wire data.
    void defer_frame() {deferred_=true;}
    bool closed() const {return closed_;}
    TransportFault fault() const {return fault_;}
    // Endpoint-requested orderly shutdown, after its queued reply is drained.
    void shutdown() {close(TransportFault::EOFClosed);}
    bool ordinary_available() const {return ordinary_.available();}
    bool control_available() const {return control_.available();}
    bool reply(bool control,const char *text,size_t length) {
        if(closed_) return false;
        return (control?control_:ordinary_).set(text,length);
    }
    // Owner callback sees one bounded complete line, validates strict UTF8/JSON
    // and chooses ordinary/control cell. No callback dispatch while both full.
    // Return false if callback cannot retain a response: terminate, no silent loss.
    // Congestion callback must abort epoch/cleanup once; it is independent of replies.
    void pump(uint64_t host,bool (*frame)(const char *,size_t,void *),
              void (*congestion)(void *),void *context,TransportIO io=TransportIO()) {
        if(closed_) return;
        if(have_host_ && host<last_host_) {close(TransportFault::ClockUncertain);return;}
        have_host_=true;last_host_=host;
        if(used_ && !completed_line_ && host-assembly_start_>=2000) {close(TransportFault::AssemblyTimeout);return;}
        // An expired reply cannot be rescued by a now-writable carrier. Check
        // before drain/admission, just as partial-line expiry precedes input.
        if(SingleOutstanding && output_waiting_ && host-output_start_>=2000) {
            close(TransportFault::OutputTimeout);return;
        }
        size_t output_budget=8192;
        // Preserve partial line writes: never interleave cells on the output pipe.
        if(ordinary_.offset) {drain(ordinary_,output_budget,io);}
        else if(control_.offset) {drain(control_,output_budget,io);}
        if(!ordinary_.offset && !control_.offset && !ordinary_.available()) drain(ordinary_,output_budget,io);
        if(closed_) return;
        if(!ordinary_.offset && !control_.available()) drain(control_,output_budget,io);
        if(closed_) return;
        bool blocked=!ordinary_.available();
        if(SingleOutstanding) {
            if(blocked && !output_waiting_) {output_waiting_=true;output_start_=host;}
            if(!blocked)output_waiting_=false;
            if(output_waiting_ && host-output_start_>=2000) {close(TransportFault::OutputTimeout);return;}
        }
        if(blocked && !congested_) {congested_=true;if(congestion) congestion(context);}
        if(!blocked) congested_=false;
        if(!ordinary_.available() && !control_.available()) return;
        if(!frame || (SingleOutstanding && blocked)) return;
        unsigned frames=0;
        if(completed_line_) {
            deferred_=false;
            if(!frame(line_,used_,context)){close(TransportFault::Write);return;}
            ++frames;
            if(deferred_)return;
            completed_line_=false;used_=0;
            if(SingleOutstanding || (!ordinary_.available() && !control_.available()))return;
        }
        // Single-byte read is deliberately simple: at most8192 calls, no read-ahead
        // that can strand a control behind an oversized unbounded discard buffer.
        for(unsigned bytes=0;bytes<8192 && frames<4;bytes++) {
            char byte;
            ssize_t result=io.read(input_,&byte,1);
            if(result<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return;
            if(result<0 || result>1) {close(TransportFault::Read);return;}
            if(result==0) {close(used_?TransportFault::PartialEOF:TransportFault::EOFClosed);return;}
            if(byte=='\n') {
                ++frames;
                completed_line_=true;deferred_=false;
                if(!frame(line_,used_,context)) {close(TransportFault::Write);return;}
                if(deferred_)return;
                completed_line_=false;used_=0;
                // Detect actual output stall before a later decoded control can
                // snapshot stale pending state. Normal enqueue is not a fault.
                if(!ordinary_.offset && !control_.offset && !ordinary_.available())
                    drain(ordinary_,output_budget,io);
                if(!ordinary_.offset && !control_.available())drain(control_,output_budget,io);
                if(closed_)return;
                // Start at the actual first blocked reply, not the next pump.
                // Otherwise sparse/no-traffic service grants an extra interval.
                if(SingleOutstanding && !ordinary_.available() && !output_waiting_) {
                    output_waiting_=true;output_start_=host;
                }
                if(!ordinary_.available() && !congested_) {
                    congested_=true;if(congestion)congestion(context);
                }
                if(SingleOutstanding || (!ordinary_.available() && !control_.available())) return;

            } else {
                if(used_==sizeof(line_)) {close(TransportFault::Oversized);return;}
                if(!used_) assembly_start_=host;
                line_[used_++]=byte;
            }
        }
    }
};
using ReplyCell=BoundedReplyCell<4096>;
using Transport=BoundedTransport<>;
using LiveTransport=BoundedTransport<65536,65536,true>;
}
#endif

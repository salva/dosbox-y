/* GPL-2.0-or-later. Values owned by one continuous execution request. */
#ifndef DOSBOX_NATIVE_STOP_CONDITIONS_H
#define DOSBOX_NATIVE_STOP_CONDITIONS_H
#include <chrono>
#include <utility>
#include <cstdint>
#include <string>
#include <vector>
namespace NativeDebug {
struct TextRegion { unsigned x=0,y=0,width=80,height=25; };
struct TextSnapshot {
    TextRegion region;
    uint64_t tick=0;
    uint32_t start=0,stride=0;
    uint16_t dos_codepage=0;
    std::vector<uint8_t> cells;
};
struct StopCondition {
    enum class Kind { Address, GuestElapsed, HostElapsed, Text, Predicate };
    Kind kind=Kind::Address;
    uint16_t cs=0,ip=0,sample_ms=10;
    uint32_t ms=0;
    TextRegion region;
    bool cp437=false;
    std::string needle;
};
struct StopEvidence {
    TextSnapshot text;
    unsigned condition_index=0,match_x=0,match_y=0;
    uint64_t start_tick=0,end_tick=0,host_elapsed_ms=0;
    bool guest_advanced=false;
    std::string error,reason;
};
struct StopRuntime {
    uint64_t last_tick=0;
    std::chrono::steady_clock::time_point host_start;
    std::vector<uint64_t> due;
    StopEvidence evidence;
    bool done=false;
    void begin(size_t count,uint64_t tick) {
        *this={};last_tick=tick;evidence.start_tick=tick;
        host_start=std::chrono::steady_clock::now();due.assign(count,tick);
    }
};
inline bool validRegion(const TextRegion &r) {
    return r.x<80 && r.y<25 && r.width && r.height &&
           r.width<=80-r.x && r.height<=25-r.y;
}
inline const char *validateConditions(const std::vector<StopCondition> &conditions) {
    if(conditions.size()>16)return "too many conditions";
    unsigned seen=0;
    for(size_t i=0;i<conditions.size();++i) {
        const auto &c=conditions[i];
        switch(c.kind) {
        case StopCondition::Kind::Address:
            for(size_t j=0;j<i;++j)
                if(conditions[j].kind==c.kind && conditions[j].cs==c.cs &&
                   conditions[j].ip==c.ip)return "duplicate address";
            break;
        case StopCondition::Kind::GuestElapsed:
        case StopCondition::Kind::HostElapsed:
            if(c.ms>86400000)return "invalid duration";
            break;
        case StopCondition::Kind::Text:
            if(!validRegion(c.region) || c.needle.empty() || c.needle.size()>80)
                return "invalid text condition";
            // fall through
        case StopCondition::Kind::Predicate:
            if(c.sample_ms<1 || c.sample_ms>1000)return "invalid sample interval";
            break;
        default:return "unknown condition";
        }
        if(c.kind!=StopCondition::Kind::Address) {
            const unsigned bit=1u<<unsigned(c.kind);
            if(seen&bit)return "duplicate condition kind";
            seen|=bit;
        }
    }
    return nullptr;
}
inline bool matchText(const StopCondition &c,const TextSnapshot &s,unsigned &x,unsigned &y) {
    for(unsigned row=0;row<c.region.height;++row)
        for(unsigned col=0;col+c.needle.size()<=c.region.width;++col) {
            size_t k=0;
            for(;k<c.needle.size();++k)
                if(s.cells[2*(row*c.region.width+col+k)]!=uint8_t(c.needle[k]))break;
            if(k==c.needle.size()){x=c.region.x+col;y=c.region.y+row;return true;}
        }
    return false;
}
// The slot owns ordering, clocks and sampling. Native seam owners supply
// call-local observations; this is not a registry or a second execution loop.
struct StopObservations {
    bool (*address)(const StopCondition &);
    bool (*text)(const TextRegion &,TextSnapshot &);
    bool (*predicate)(bool &error);
};
inline bool evaluateConditions(const std::vector<StopCondition> &conditions,
                               StopRuntime &runtime,uint64_t tick,bool advanced,
                               const StopObservations &observe) {
    if(conditions.empty() || runtime.done)return runtime.done;
    auto &r=runtime.evidence;r.end_tick=tick;r.guest_advanced=advanced;
    const auto hostElapsed=[&]() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-runtime.host_start).count();
    };
    r.host_elapsed_ms=hostElapsed();
    if(tick<runtime.last_tick)r.error="clock_discontinuity";
    runtime.last_tick=tick;
    for(size_t i=0;r.error.empty() && i<conditions.size();++i) {
        const auto &c=conditions[i];bool match=false;
        switch(c.kind) {
        case StopCondition::Kind::Address:match=observe.address(c);break;
        case StopCondition::Kind::GuestElapsed:match=tick-r.start_tick>=c.ms;break;
        case StopCondition::Kind::HostElapsed:
            r.host_elapsed_ms=hostElapsed();match=r.host_elapsed_ms>=c.ms;break;
        case StopCondition::Kind::Text:
            if(tick<runtime.due[i])continue;
            runtime.due[i]=tick+c.sample_ms;
            if(!observe.text(c.region,r.text))r.error="condition_unavailable";
            else match=matchText(c,r.text,r.match_x,r.match_y);
            break;
        case StopCondition::Kind::Predicate: {
            if(tick<runtime.due[i])continue;
            runtime.due[i]=tick+c.sample_ms;
            bool error=false;match=observe.predicate(error);
            if(error)r.error="observer_error";
            // Reconcile host expiry at callback settlement, without replaying
            // observers. Earlier host entries beat a later true predicate;
            // sticky observer errors retain priority over elapsed conditions.
            r.host_elapsed_ms=hostElapsed();
            if(!error)for(size_t j=0;j<i;++j) {
                if(conditions[j].kind==StopCondition::Kind::HostElapsed &&
                   r.host_elapsed_ms>=conditions[j].ms) {
                    r.condition_index=j+1;r.reason="condition";
                    runtime.done=true;return true;
                }
            }
            break;
        }
        }
        if(match || !r.error.empty()) {
            r.host_elapsed_ms=hostElapsed();
            r.condition_index=i+1;r.reason="condition";runtime.done=true;return true;
        }
    }
    if(!r.error.empty()){runtime.done=true;return true;}
    return false;
}
}
bool VGA_CopyTextSnapshot(const NativeDebug::TextRegion &,NativeDebug::TextSnapshot &);
#endif

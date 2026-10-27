/* GPL-2.0-or-later. Strict built-in wire projection of native stop conditions. */
#ifndef DOSBOX_AGENT_STOP_CONDITIONS_H
#define DOSBOX_AGENT_STOP_CONDITIONS_H
#include "agent/agent_protocol.h"
#include "native_stop_conditions.h"
#include "cp437_uni.h"
#include <initializer_list>
namespace dosbox_agent {
inline bool StopFields(const JsonValue &v,std::initializer_list<const char *> names) {
    if(v.type!=JsonType::Object)return false;
    for(const auto &field:v.object) {
        bool found=false;for(auto name:names)if(field.first==name)found=true;
        if(!found)return false;
    }
    return true;
}
inline bool StopInteger(const JsonValue *v,unsigned max,unsigned &out) {
    if(!v || v->type!=JsonType::Number || v->text.empty())return false;
    unsigned n=0;
    for(char c:v->text) {if(c<'0'||c>'9'||n>max/10)return false;n=n*10+(c-'0');if(n>max)return false;}
    out=n;return true;
}
inline bool ParseStopConditions(const JsonValue *list,std::vector<NativeDebug::StopCondition> &out,std::string &error) {
    out.clear();
    if(!list)return true;
    const auto fail=[&]() {error="invalid stop_any built-in condition";return false;};
    if(list->type!=JsonType::Array || list->array.size()>16)return fail();
    for(const auto &v:list->array) {
        NativeDebug::StopCondition c;
        const auto *kind=v.Find("kind");
        if(!kind || kind->type!=JsonType::String)return fail();
        unsigned n=0;
        if(kind->text=="address") {
            if(!StopFields(v,{"kind","cs","ip"}) || !StopInteger(v.Find("cs"),65535,n))return fail();
            c.cs=n;
            if(!StopInteger(v.Find("ip"),65535,n))return fail();
            c.ip=n;
        } else if(kind->text=="elapsed") {
            if(!StopFields(v,{"kind","ms","clock"}) || !StopInteger(v.Find("ms"),86400000,n))return fail();
            c.ms=n;
            c.kind=NativeDebug::StopCondition::Kind::GuestElapsed;
            if(const auto *clock=v.Find("clock")) {
                if(clock->type!=JsonType::String)return fail();
                if(clock->text=="host_monotonic")c.kind=NativeDebug::StopCondition::Kind::HostElapsed;
                else if(clock->text!="guest_ms")return fail();
            }
        } else if(kind->text=="text") {
            if(!StopFields(v,{"kind","needle","encoding","region","sample_guest_ms"}))return fail();
            c.kind=NativeDebug::StopCondition::Kind::Text;
            const auto *encoding=v.Find("encoding"),*needle=v.Find("needle");
            if(!encoding || encoding->type!=JsonType::String || !needle)return fail();
            if(encoding->text=="raw") {
                if(needle->type!=JsonType::Array || needle->array.size()>80)return fail();
                for(const auto &b:needle->array) {if(!StopInteger(&b,255,n))return fail();c.needle.push_back(char(n));}
            } else if(encoding->text=="cp437") {
                c.cp437=true;
                if(needle->type!=JsonType::String)return fail();
                const auto &s=needle->text;
                for(size_t p=0;p<s.size();) {
                    uint32_t u=uint8_t(s[p++]),min=0;unsigned tail=0;
                    if(u>=0xc2 && u<=0xdf){u&=31;tail=1;min=0x80;}
                    else if(u>=0xe0 && u<=0xef){u&=15;tail=2;min=0x800;}
                    else if(u>=0xf0 && u<=0xf4){u&=7;tail=3;min=0x10000;}
                    else if(u>=0x80)return fail();
                    while(tail--){if(p==s.size() || (uint8_t(s[p])&0xc0)!=0x80)return fail();u=(u<<6)|(uint8_t(s[p++])&63);}
                    if(u<min || u>0x10ffff || (u>=0xd800 && u<=0xdfff))return fail();
                    unsigned b=0;for(;b<256;++b)if((cp437_to_unicode[b]?cp437_to_unicode[b]:0xfffd)==u)break;
                    if(b==256)return fail();
                    c.needle.push_back(char(b));
                    if(c.needle.size()>80)return fail();
                }
            } else return fail();
            if(const auto *sample=v.Find("sample_guest_ms")) {if(!StopInteger(sample,1000,n)||!n)return fail();c.sample_ms=n;}
            if(const auto *region=v.Find("region")) {
                if(!StopFields(*region,{"x","y","width","height"}))return fail();
                if(!StopInteger(region->Find("x"),79,n))return fail();
                c.region.x=n;
                if(!StopInteger(region->Find("y"),24,n))return fail();
                c.region.y=n;
                if(!StopInteger(region->Find("width"),80,n))return fail();
                c.region.width=n;
                if(!StopInteger(region->Find("height"),25,n))return fail();
                c.region.height=n;
            }
        } else return fail();
        out.push_back(std::move(c));
    }
    if(const char *why=NativeDebug::validateConditions(out)){error=why;return false;}
    return true;
}
}
#endif

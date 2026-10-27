/* GPL-2.0-or-later. Lua-only worker; native operations use owned messages. */
#include "lua_driver_worker.h"
#include <time.h>
#include <cstring>
#include <chrono>
#include <map>
#include "cp437_uni.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
namespace LuaDriver {
namespace {
struct Worker {
    const Options &options;
    Handoff &handoff;
    bool closing=false, observing=false;
    bool close_interrupted=false;
    bool budget_failed=false;
    lua_Hook previous_hook=nullptr;
    int previous_mask=0, previous_count=0;
    uint32_t instructions=0;
    std::chrono::steady_clock::time_point observation_start;
    uint64_t ordinal=0, next_registration=0;
    std::map<uint64_t,int> observers;
    lua_CFunction library_calls[6]={};
    lua_CFunction wrapped_call=nullptr;
    Worker(const Options &selected,Handoff &channel):options(selected),handoff(channel) {}
};
const char *const context_type="dosbox.local.context";
// Cooperative cancellation of Lua bytecode. This does not preempt a foreign
// blocking C function; bindings and native effects retain their own settlement.
void closeHook(lua_State *L,lua_Debug *) {
    auto *w=*static_cast<Worker **>(lua_getextraspace(L));
    if(w && !w->close_interrupted && w->handoff.closing()) {
        // Disarm before raising: protected error handling must be able to
        // unwind without another close exception. Effect admission remains
        // revoked by the handoff, independently of this bytecode interrupt.
        w->close_interrupted=true;
        luaL_error(L,"driver closing");
    }
    if(w && w->observing) {
        w->instructions+=1000;
        if(w->instructions>=100000 || std::chrono::steady_clock::now()-w->observation_start>=std::chrono::milliseconds(100)) {
            w->budget_failed=true;w->handoff.observerViolation();
            luaL_error(L,"predicate_budget_exceeded");
        }
    }
}
int encode(lua_State *L) {
    const auto &r=*static_cast<Response *>(lua_touserdata(L,1));
    auto op=static_cast<Operation>(lua_tointeger(L,2));
    if(op==Operation::TextSnapshot && r.error.empty()) {
        lua_newtable(L);
        lua_pushlstring(L,reinterpret_cast<const char *>(r.text.cells.data()),r.text.cells.size());
        lua_setfield(L,-2,"cells");
        lua_pushinteger(L,r.text.tick);lua_setfield(L,-2,"tick");
        lua_pushinteger(L,r.text.start);lua_setfield(L,-2,"display_start");
        lua_pushinteger(L,r.text.stride);lua_setfield(L,-2,"stride");
        lua_pushinteger(L,r.text.dos_codepage);lua_setfield(L,-2,"dos_codepage");
        lua_pushinteger(L,r.text.region.x);lua_setfield(L,-2,"x");
        lua_pushinteger(L,r.text.region.y);lua_setfield(L,-2,"y");
        lua_pushinteger(L,r.text.region.width);lua_setfield(L,-2,"width");
        lua_pushinteger(L,r.text.region.height);lua_setfield(L,-2,"height");
        luaL_Buffer raw;luaL_buffinit(L,&raw);
        for(size_t i=0;i<r.text.cells.size();i+=2)luaL_addchar(&raw,char(r.text.cells[i]));
        luaL_pushresult(&raw);lua_setfield(L,-2,"raw");
        luaL_Buffer cp437;luaL_buffinit(L,&cp437);
        for(size_t i=0;i<r.text.cells.size();i+=2) {
            const auto b=r.text.cells[i];
            const uint32_t u=cp437_to_unicode[b]?cp437_to_unicode[b]:0xfffd;
            if(u<0x80)luaL_addchar(&cp437,char(u));
            else if(u<0x800) {luaL_addchar(&cp437,char(0xc0|(u>>6)));luaL_addchar(&cp437,char(0x80|(u&63)));}
            else {luaL_addchar(&cp437,char(0xe0|(u>>12)));luaL_addchar(&cp437,char(0x80|((u>>6)&63)));luaL_addchar(&cp437,char(0x80|(u&63)));}
        }
        luaL_pushresult(&cp437);lua_setfield(L,-2,"cp437");
        return 1;
    }
    if(!r.error.empty() && !(op==Operation::Save && r.save.valid)) {
        lua_pushlstring(L,r.error.data(),r.error.size());return lua_error(L);
    }
    if(op==Operation::Read) {
        lua_pushlstring(L,r.bytes.empty()?"":reinterpret_cast<const char *>(r.bytes.data()),r.bytes.size());return 1;
    }
    if(op==Operation::StateReady) {
        lua_newtable(L);
        lua_pushboolean(L,r.state_reasons.empty());lua_setfield(L,-2,"ready");
        lua_pushlstring(L,r.state_profile.data(),r.state_profile.size());lua_setfield(L,-2,"profile");
        lua_newtable(L);
        for(size_t i=0;i<r.state_reasons.size();++i) {
            lua_pushlstring(L,r.state_reasons[i].data(),r.state_reasons[i].size());
            lua_rawseti(L,-2,i+1);
        }
        lua_setfield(L,-2,"reasons");return 1;
    }
    if(op!=Operation::Context) {
        lua_newtable(L);
        lua_pushinteger(L,r.condition_index);lua_setfield(L,-2,"condition_index");
        lua_pushinteger(L,r.match_x);lua_setfield(L,-2,"match_x");
        lua_pushinteger(L,r.match_y);lua_setfield(L,-2,"match_y");
        if(!r.text.cells.empty()) {
            lua_pushlstring(L,reinterpret_cast<const char *>(r.text.cells.data()),r.text.cells.size());
            lua_setfield(L,-2,"text_cells");
            lua_pushinteger(L,r.text.tick);lua_setfield(L,-2,"text_tick");
            lua_pushinteger(L,r.text.dos_codepage);lua_setfield(L,-2,"dos_codepage");
        }
        lua_pushinteger(L,r.start_tick);lua_setfield(L,-2,"start_tick");
        lua_pushinteger(L,r.end_tick);lua_setfield(L,-2,"end_tick");
        lua_pushinteger(L,r.host_elapsed_ms);lua_setfield(L,-2,"host_elapsed_ms");
        lua_pushboolean(L,r.guest_advanced);lua_setfield(L,-2,"guest_advanced");
        if(!r.error.empty()) {lua_pushlstring(L,r.error.data(),r.error.size());lua_setfield(L,-2,"error");}
    }
    if(r.context.invocation && r.context.checkpoint &&
       !(op==Operation::Save && r.save.valid && !r.save.usable)) {
        auto *c=static_cast<NativeDebug::Context *>(lua_newuserdatauv(L,sizeof(r.context),0));
        *c=r.context;luaL_setmetatable(L,context_type);
    } else lua_pushnil(L);
    if(op==Operation::Context)return 1;
    lua_setfield(L,-2,"context");
    if(r.registration) {
        lua_pushinteger(L,r.registration);lua_setfield(L,-2,"registration");
    }
    lua_pushlstring(L,r.reason.data(),r.reason.size());lua_setfield(L,-2,"reason");
    lua_pushlstring(L,r.boundary.data(),r.boundary.size());lua_setfield(L,-2,"boundary");
    lua_pushboolean(L,r.continuation);lua_setfield(L,-2,"continuation");
    lua_pushboolean(L,r.registers_valid);lua_setfield(L,-2,"registers_valid");
    lua_pushboolean(L,r.partial);lua_setfield(L,-2,"partial");
    if(op==Operation::Save && r.save.valid) {
        // Post-effect failures retain typed publication/cleanup facts rather
        // than throwing away the response. Native retirement governs success.
        lua_newtable(L);
        const struct {const char *name;bool value;} effects[]={
            {"completed",r.save.completed},{"published",r.save.published},
            {"durable",r.save.durable},{"cleanup_attempted",r.save.cleanup_attempted},
            {"cleanup_failed",r.save.cleanup_failed},{"poisoned",r.save.poisoned},
            {"retired",r.save.retired},{"usable",r.save.usable}};
        for(auto effect:effects){lua_pushboolean(L,effect.value);lua_setfield(L,-2,effect.name);}
        lua_pushinteger(L,r.save.archive_bytes);lua_setfield(L,-2,"archive_bytes");
        lua_pushlstring(L,r.save.archive_sha256.data(),r.save.archive_sha256.size());
        lua_setfield(L,-2,"archive_sha256");
        lua_pushlstring(L,r.save.code.data(),r.save.code.size());lua_setfield(L,-2,"code");
        lua_setfield(L,-2,"effects");
        if(!r.error.empty()) {
            lua_pushlstring(L,r.error.data(),r.error.size());lua_setfield(L,-2,"error");
        }
    }
    if(r.registers_valid) {
        lua_newtable(L);
        const struct {const char *name;uint32_t value;} regs[]={
            {"cs",r.cs},{"ip",r.ip},{"ax",r.ax},{"bx",r.bx},{"cx",r.cx},{"dx",r.dx},
            {"eax",r.eax},{"ebx",r.ebx},{"ecx",r.ecx},{"edx",r.edx},
            {"esi",r.esi},{"edi",r.edi},{"ebp",r.ebp},{"esp",r.esp},
            {"flags",r.flags},{"ds",r.ds},{"es",r.es},{"fs",r.fs},{"gs",r.gs},{"ss",r.ss}};
        for(auto reg:regs){lua_pushinteger(L,reg.value);lua_setfield(L,-2,reg.name);}
        lua_pushboolean(L,r.code_big);lua_setfield(L,-2,"code_big");
        lua_setfield(L,-2,"registers");
    }
    return 1;
}
NativeDebug::Context checkContext(lua_State *L) {
    auto *c=static_cast<NativeDebug::Context *>(luaL_checkudata(L,1,context_type));
    if(lua_rawlen(L,1)!=sizeof(*c))luaL_error(L,"invalid context storage");
    return *c;
}
// Protected callback execution on the worker only. Native event authority is
// not inferred here; observerRead must still be admitted at the native seam.
int observe(lua_State *L) {
    auto &w=*static_cast<Worker *>(lua_touserdata(L,1));
    const auto &event=*static_cast<const Event *>(lua_touserdata(L,2));
    const auto found=w.observers.find(event.registration);
    if(found==w.observers.end())return luaL_error(L,"unknown observer registration");
    lua_rawgeti(L,LUA_REGISTRYINDEX,found->second);
    lua_pushcfunction(L,encode);lua_pushlightuserdata(L,const_cast<Response *>(&event.image));
    lua_pushinteger(L,static_cast<lua_Integer>(Operation::Registers));
    lua_call(L,2,1);lua_call(L,1,1);
    if(!lua_isboolean(L,-1))return luaL_error(L,"observer must return a boolean");
    return 1;
}
Verdict observation(lua_State *L,Worker &w,const Event &event) {
    const int top=lua_gettop(L);
    w.observing=true;w.ordinal=event.ordinal;
    w.previous_hook=lua_gethook(L);w.previous_mask=lua_gethookmask(L);w.previous_count=lua_gethookcount(L);
    lua_sethook(L,closeHook,LUA_MASKCOUNT,1000);
    w.instructions=0;w.budget_failed=false;w.observation_start=std::chrono::steady_clock::now();
    lua_pushcfunction(L,observe);lua_pushlightuserdata(L,&w);
    lua_pushlightuserdata(L,const_cast<Event *>(&event));
    const int rc=lua_pcall(L,2,1,0);
    Verdict verdict;verdict.error=rc!=LUA_OK || w.budget_failed ||
        std::chrono::steady_clock::now()-w.observation_start>=std::chrono::milliseconds(100);
    verdict.propagate=verdict.error || lua_toboolean(L,-1);
    lua_settop(L,top);w.ordinal=0;w.observing=false;
    lua_sethook(L,w.previous_hook,w.previous_mask,w.previous_count);
    return verdict;
}
// Strict schema parsing is protected by invoke's caller before owned requests.
void fields(lua_State *L,int table,const char *allowed) {
    table=lua_absindex(L,table);luaL_checktype(L,table,LUA_TTABLE);
    if(lua_getmetatable(L,table))luaL_error(L,"plain table required");
    lua_pushnil(L);
    while(lua_next(L,table)) {
        if(lua_type(L,-2)!=LUA_TSTRING)luaL_error(L,"unknown field");
        size_t length=0;const char *key=lua_tolstring(L,-2,&length);
        bool found=false;
        for(const char *p=allowed;*p;) {
            const char *end=strchr(p+1,'|');if(!end)break;
            if(size_t(end-p-1)==length && !memcmp(p+1,key,length))found=true;
            p=end+1;
        }
        if(!found)luaL_error(L,"unknown field: %s",key);
        lua_pop(L,1);
    }
}
const char *schemaString(lua_State *L,int index) {
    if(lua_type(L,index)!=LUA_TSTRING)luaL_error(L,"schema string required");
    size_t length=0;const char *text=lua_tolstring(L,index,&length);
    if(memchr(text,0,length))luaL_error(L,"invalid schema string");
    return text;
}
lua_Integer integerField(lua_State *L,int t,const char *key,lua_Integer fallback,lua_Integer max) {
    lua_getfield(L,t,key);
    const auto v=lua_isnil(L,-1)?fallback:lua_isinteger(L,-1)?lua_tointeger(L,-1):-1;
    lua_pop(L,1);if(v<0 || v>max)luaL_error(L,"invalid %s",key);return v;
}
void region(lua_State *L,int t,NativeDebug::TextRegion &r) {
    t=lua_absindex(L,t);fields(L,t,"|x||y||width||height|");
    r.x=integerField(L,t,"x",0,79);r.y=integerField(L,t,"y",0,24);
    r.width=integerField(L,t,"width",80,80);r.height=integerField(L,t,"height",25,25);
    if(!NativeDebug::validRegion(r))luaL_error(L,"invalid region");
}
int parseSnapshot(lua_State *L) {
    auto &request=*static_cast<Request *>(lua_touserdata(L,1));
    if(!lua_isnil(L,2))region(L,2,request.region);
    return 0;
}
// Called in a protected trampoline: longjmp never crosses an owned Request.
int parseRun(lua_State *L) {
    auto &request=*static_cast<Request *>(lua_touserdata(L,1));
    if(lua_isnil(L,2))return 0;
    fields(L,2,"|stop_any|");lua_getfield(L,2,"stop_any");
    if(lua_isnil(L,-1))return 0;
    luaL_checktype(L,-1,LUA_TTABLE);const int list=lua_gettop(L);
    if(lua_getmetatable(L,list))return luaL_error(L,"plain condition list required");
    const size_t n=lua_rawlen(L,list);if(n>16)return luaL_error(L,"too many conditions");
    lua_pushnil(L);size_t count=0;
    while(lua_next(L,list)) {
        if(!lua_isinteger(L,-2) || lua_tointeger(L,-2)<1 ||
           uint64_t(lua_tointeger(L,-2))>n)return luaL_error(L,"invalid condition list");
        ++count;lua_pop(L,1);
    }
    if(count!=n)return luaL_error(L,"sparse condition list");
    request.stop_any.resize(n);
    for(size_t i=0;i<n;++i) {
        lua_rawgeti(L,list,i+1);const int t=lua_gettop(L);
        luaL_checktype(L,t,LUA_TTABLE);
        if(lua_getmetatable(L,t))return luaL_error(L,"plain condition required");
        lua_getfield(L,t,"kind");const char *kind=schemaString(L,-1);lua_pop(L,1);
        auto &c=request.stop_any[i];
        if(!strcmp(kind,"elapsed")) {
            fields(L,t,"|kind||clock||ms|");lua_getfield(L,t,"clock");
            const char *clock=schemaString(L,-1);
            if(!strcmp(clock,"guest_ticks"))c.kind=NativeDebug::StopCondition::Kind::GuestElapsed;
            else if(!strcmp(clock,"host_monotonic"))c.kind=NativeDebug::StopCondition::Kind::HostElapsed;
            else return luaL_error(L,"unsupported clock");
            lua_pop(L,1);c.ms=integerField(L,t,"ms",-1,86400000);
        } else if(!strcmp(kind,"address")) {
            fields(L,t,"|kind||cs||ip|");c.kind=NativeDebug::StopCondition::Kind::Address;
            c.cs=integerField(L,t,"cs",-1,65535);c.ip=integerField(L,t,"ip",-1,65535);
        } else if(!strcmp(kind,"text")) {
            fields(L,t,"|kind||region||needle||encoding||sample_guest_ms|");
            c.kind=NativeDebug::StopCondition::Kind::Text;
            lua_getfield(L,t,"region");if(!lua_isnil(L,-1))region(L,-1,c.region);lua_pop(L,1);
            c.sample_ms=integerField(L,t,"sample_guest_ms",10,1000);
            lua_getfield(L,t,"encoding");const char *enc=schemaString(L,-1);
            if(strcmp(enc,"raw") && strcmp(enc,"cp437"))return luaL_error(L,"unsupported encoding");
            c.cp437=!strcmp(enc,"cp437");lua_pop(L,1);
            lua_getfield(L,t,"needle");
            if(lua_type(L,-1)!=LUA_TSTRING)return luaL_error(L,"needle must be a string");
            size_t len=0;const auto *s=reinterpret_cast<const unsigned char *>(lua_tolstring(L,-1,&len));
            if(!c.cp437)c.needle.assign(reinterpret_cast<const char *>(s),len);
            else for(size_t p=0;p<len;) {
                uint32_t u=s[p++];unsigned tail=0;uint32_t min=0;
                if(u>=0xc2 && u<=0xdf){u&=31;tail=1;min=0x80;}
                else if(u>=0xe0 && u<=0xef){u&=15;tail=2;min=0x800;}
                else if(u>=0xf0 && u<=0xf4){u&=7;tail=3;min=0x10000;}
                else if(u>=0x80)return luaL_error(L,"invalid UTF-8");
                while(tail--){if(p==len || (s[p]&0xc0)!=0x80)return luaL_error(L,"invalid UTF-8");u=(u<<6)|(s[p++]&63);}
                if(u<min || u>0x10ffff || (u>=0xd800 && u<=0xdfff))return luaL_error(L,"invalid UTF-8");
                unsigned b=0;for(;b<256;++b)if((cp437_to_unicode[b]?cp437_to_unicode[b]:0xfffd)==u)break;
                if(b==256)return luaL_error(L,"unrepresentable CP437 needle");
                c.needle.push_back(char(b));
            }
            lua_pop(L,1);
        } else if(!strcmp(kind,"predicate")) {
            fields(L,t,"|kind||test||sample_guest_ms|");c.kind=NativeDebug::StopCondition::Kind::Predicate;
            c.sample_ms=integerField(L,t,"sample_guest_ms",10,1000);
            lua_getfield(L,t,"test");luaL_checktype(L,-1,LUA_TFUNCTION);lua_pop(L,1);
        } else return luaL_error(L,"unknown condition kind");
        lua_pop(L,1);
    }
    if(const char *error=NativeDebug::validateConditions(request.stop_any))return luaL_error(L,"%s",error);
    return 0;
}
int invoke(lua_State *L) {
    auto &w=*static_cast<Worker *>(lua_touserdata(L,lua_upvalueindex(1)));
    auto op=static_cast<Operation>(lua_tointeger(L,lua_upvalueindex(2)));
    // Clock/read boundaries share the observation's cooperative budget.
    if(w.observing && std::chrono::steady_clock::now()-w.observation_start>=std::chrono::milliseconds(100)) {
        w.budget_failed=true;w.handoff.observerViolation();
        return luaL_error(L,"predicate_budget_exceeded");
    }
    if(w.observing && op!=Operation::Read && op!=Operation::TextSnapshot) {
        w.handoff.observerViolation();return luaL_error(L,"observer effect refused");
    }
    // Validation errors are sticky even if callback code catches the error.
    if(w.observing && op==Operation::Read && (lua_gettop(L)!=3 || !luaL_testudata(L,1,context_type) ||
        lua_rawlen(L,1)!=sizeof(NativeDebug::Context) || !lua_isinteger(L,2) ||
        !lua_isinteger(L,3) || lua_tointeger(L,2)<0 ||
        uint64_t(lua_tointeger(L,2))>UINT32_MAX || lua_tointeger(L,3)<1 ||
        lua_tointeger(L,3)>4096 ||
        uint64_t(lua_tointeger(L,2))+uint64_t(lua_tointeger(L,3))>uint64_t(UINT32_MAX)+1))
        w.handoff.observerViolation();
    if(w.observing && op==Operation::TextSnapshot &&
       (lua_gettop(L)<1 || lua_gettop(L)>2 || !luaL_testudata(L,1,context_type) ||
        lua_rawlen(L,1)!=sizeof(NativeDebug::Context)))w.handoff.observerViolation();
    if(w.closing || w.handoff.closing())return luaL_error(L,"driver closing");
    // Validate Lua values before creating nontrivial C++ objects.
    NativeDebug::Context context;
    lua_Integer address=0,length=0,cs=0,ip=0,timeout=30000;int argc=0;
    const char *path=nullptr;size_t path_size=0;
    uint64_t registration=0;int callback=LUA_NOREF;
    if(op==Operation::Breakpoint || op==Operation::Watchpoint) {
        cs=luaL_checkinteger(L,1);ip=luaL_checkinteger(L,2);argc=lua_gettop(L)==3?3:2;
        const uint64_t max_offset=op==Operation::Watchpoint?UINT32_MAX:65535;
        if(cs<0 || cs>65535 || ip<0 || uint64_t(ip)>max_offset)
            return luaL_error(L,"invalid registration address");
        if(argc==3)luaL_checktype(L,3,LUA_TFUNCTION);
        if(w.next_registration>=uint64_t(LUA_MAXINTEGER))return luaL_error(L,"registration exhausted");
        registration=++w.next_registration;
    }
    if(op==Operation::Remove) {
        const auto id=luaL_checkinteger(L,1);argc=1;
        if(id<=0)return luaL_error(L,"invalid registration");
        registration=uint64_t(id);
    }
    if(op==Operation::Save) {
        path=luaL_checklstring(L,1,&path_size);argc=lua_gettop(L)==2?2:1;
        if(argc==2)timeout=luaL_checkinteger(L,2);
        if(!path_size || path_size>4096 || memchr(path,0,path_size) || timeout<1 || timeout>30000)
            return luaL_error(L,"invalid save arguments");
    }
    if(op==Operation::RunUntil) {
        context=checkContext(L);
        cs=luaL_checkinteger(L,2);ip=luaL_checkinteger(L,3);argc=3;
        if(cs<0 || cs>65535 || ip<0 || ip>65535)return luaL_error(L,"invalid target");
    }
    if(op==Operation::Registers || op==Operation::Read) {
        context=checkContext(L);argc=1;
    }
    if(op==Operation::Read) {
        address=luaL_checkinteger(L,2);length=luaL_checkinteger(L,3);argc=3;
        if(address<0 || uint64_t(address)>UINT32_MAX || length<1 || length>4096 ||
           uint64_t(address)+uint64_t(length)>uint64_t(UINT32_MAX)+1)
            return luaL_error(L,"invalid read range");
    }
    if(op==Operation::Run)argc=lua_gettop(L)==1?1:0;
    if(op==Operation::TextSnapshot) {context=checkContext(L);argc=lua_gettop(L)==2?2:1;}
    if(lua_gettop(L)!=argc)return luaL_error(L,"invalid argument count");
    if((op==Operation::Breakpoint || op==Operation::Watchpoint) && argc==3) {
        lua_pushvalue(L,3);callback=luaL_ref(L,LUA_REGISTRYINDEX);
    }
    // Build the encoding closure before response ownership starts. All Lua
    // allocations while owned values are alive occur inside protected encode.
    lua_pushcfunction(L,encode);
    int rc=LUA_OK;
    bool failed=false;
    try {
        Request request;Response response;
        request.operation=op;request.context=context;
        request.address=uint32_t(address);request.length=uint32_t(length);
        request.cs=uint16_t(cs);request.ip=uint16_t(ip);request.timeout_ms=uint32_t(timeout);
        if(op==Operation::Watchpoint)request.address=uint32_t(ip);
        request.registration=registration;request.observer=callback!=LUA_NOREF;
        if(op==Operation::TextSnapshot) {
            lua_pushcfunction(L,parseSnapshot);lua_pushlightuserdata(L,&request);
            if(argc==2)lua_pushvalue(L,2);else lua_pushnil(L);
            if(lua_pcall(L,2,0,0)!=LUA_OK) {
                const char *error=lua_tostring(L,-1);response.error=error?error:"invalid snapshot";lua_pop(L,1);
                if(w.observing)w.handoff.observerViolation();
            }
        }
        if(op==Operation::Run) {
            lua_pushcfunction(L,parseRun);lua_pushlightuserdata(L,&request);
            if(argc)lua_pushvalue(L,1);else lua_pushnil(L);
            if(lua_pcall(L,2,0,0)!=LUA_OK) {
                const char *error=lua_tostring(L,-1);response.error=error?error:"invalid Run";lua_pop(L,1);
            }
            if(response.error.empty())for(size_t i=0;i<request.stop_any.size();++i)
                if(request.stop_any[i].kind==NativeDebug::StopCondition::Kind::Predicate) {
                    lua_getfield(L,1,"stop_any");lua_rawgeti(L,-1,i+1);lua_getfield(L,-1,"test");
                    callback=luaL_ref(L,LUA_REGISTRYINDEX);lua_pop(L,2);
                    w.observers.emplace(0,callback);
                }
        }
        if(path)request.path.assign(path,path_size);
        try {
            const bool ok=!response.error.empty() || (w.observing?w.handoff.observerRead(w.ordinal,std::move(request),response):
                w.handoff.call(std::move(request),response,[&](const Event &event){return observation(L,w,event);}));
            if(!ok)response.error="driver closing";
        }
        catch (...) {response.error="worker handoff failed";}
        if(response.error.empty() && (op==Operation::Breakpoint || op==Operation::Watchpoint)) {
            if(response.registration!=registration)response.error="registration identity mismatch";
            else if(callback!=LUA_NOREF) {
                w.observers.emplace(registration,callback);callback=LUA_NOREF;
            }
        }
        if(response.error.empty() && op==Operation::Remove) {
            const auto found=w.observers.find(registration);
            if(found!=w.observers.end()) {
                luaL_unref(L,LUA_REGISTRYINDEX,found->second);w.observers.erase(found);
            }
        }
        lua_pushlightuserdata(L,&response);lua_pushinteger(L,static_cast<lua_Integer>(op));
        rc=lua_pcall(L,2,1,0);
    } catch (...) {failed=true;}
    if(op==Operation::Run)w.observers.erase(0);
    if(callback!=LUA_NOREF)luaL_unref(L,LUA_REGISTRYINDEX,callback);
    if(failed)return luaL_error(L,"worker host exception");
    if(rc!=LUA_OK)return lua_error(L);
    return 1;
}
int module(lua_State *L) {
    lua_newtable(L);
    const struct {const char *name;Operation op;} bindings[]={
        {"text_snapshot",Operation::TextSnapshot},
        {"context",Operation::Context},{"registers",Operation::Registers},
        {"read",Operation::Read},{"step",Operation::Step},{"run",Operation::Run},{"over",Operation::Over},
        {"run_until",Operation::RunUntil},{"save",Operation::Save},{"state_ready",Operation::StateReady},
        {"breakpoint",Operation::Breakpoint},{"watchpoint",Operation::Watchpoint},
        {"remove",Operation::Remove}};
    for(auto b:bindings) {
        lua_pushvalue(L,lua_upvalueindex(1));lua_pushinteger(L,static_cast<lua_Integer>(b.op));
        lua_pushcclosure(L,invoke,2);lua_setfield(L,-2,b.name);
    }
    return 1;
}
int guardedWrappedCall(lua_State *L) {
    auto *w=*static_cast<Worker **>(lua_getextraspace(L));
    if(w->observing) {
        w->handoff.observerViolation();return luaL_error(L,"observer coroutine/hook transfer refused");
    }
    return w->wrapped_call(L);
}
template<unsigned Index> int guardedLibraryCall(lua_State *L) {
    auto *w=*static_cast<Worker **>(lua_getextraspace(L));
    if(w->observing) {
        w->handoff.observerViolation();return luaL_error(L,"observer coroutine/hook transfer refused");
    }
    const int results=w->library_calls[Index](L);
    if(Index==3) {
        // Stock wrap returns a C closure over its thread. Retain only the
        // thread in the public closure, never the unguarded callable binding.
        w->wrapped_call=lua_tocfunction(L,-1);
        lua_getupvalue(L,-1,1);
        lua_pushcclosure(L,guardedWrappedCall,1);
        lua_replace(L,-2);
    }
    return results;
}
template<unsigned Index> void guardLibrary(lua_State *L,Worker &w,const char *library,const char *name) {
    lua_getglobal(L,library);lua_getfield(L,-1,name);
    w.library_calls[Index]=lua_tocfunction(L,-1);lua_pop(L,1);
    lua_pushcfunction(L,guardedLibraryCall<Index>);lua_setfield(L,-2,name);lua_pop(L,1);
}
int entry(lua_State *L) {
    auto &w=*static_cast<Worker *>(lua_touserdata(L,1));
    luaL_openlibs(L);
    guardLibrary<0>(L,w,"debug","sethook");
    guardLibrary<1>(L,w,"coroutine","resume");guardLibrary<2>(L,w,"coroutine","yield");
    guardLibrary<3>(L,w,"coroutine","wrap");guardLibrary<4>(L,w,"coroutine","create");
    guardLibrary<5>(L,w,"coroutine","close");luaL_newmetatable(L,context_type);
    lua_pushliteral(L,"opaque native context");lua_setfield(L,-2,"__metatable");lua_pop(L,1);
    lua_getglobal(L,"package");lua_getfield(L,-1,"preload");
    lua_pushlightuserdata(L,&w);lua_pushcclosure(L,module,1);lua_setfield(L,-2,"dosbox");lua_pop(L,2);
    static const char helpers[]=R"lua(
local d=require('dosbox')
local run=d.run
function d.run_until_timeout(ms, options)
  if options~=nil then
    assert(type(options)=='table','options must be a table')
    for k in pairs(options) do assert(k=='clock','unknown option') end
  end
  return run{stop_any={{kind='elapsed',ms=ms,clock=options and options.clock or 'guest_ticks'}}}
end
function d.run_until_text(needle, options)
  local c={kind='text',needle=needle,encoding='cp437'}
  if options~=nil then
    assert(type(options)=='table','options must be a table')
    for k,v in pairs(options) do
      assert(k=='encoding' or k=='region' or k=='sample_guest_ms','unknown option')
      c[k]=v
    end
  end
  return run{stop_any={c}}
end
)lua";
    if(luaL_loadbuffer(L,helpers,sizeof(helpers)-1,"stop helpers")!=LUA_OK)return lua_error(L);
    lua_call(L,0,0);
    const int rc=w.options.script==Options::Script::File?luaL_loadfile(L,w.options.source.c_str()):
        luaL_loadbuffer(L,w.options.source.data(),w.options.source.size(),"=(lua-driver-code)");
    if(rc!=LUA_OK)return lua_error(L);
    lua_call(L,0,0);return 0;
}
}
void runWorker(const Options &options,Handoff &handoff,WorkerResult &result) {
    result={};Worker worker{options,handoff};lua_State *L=nullptr;
    try {
        if(handoff.waitReady()) {
            L=luaL_newstate();
            if(!L)result.error="Lua allocation failed";
            else {
                *static_cast<Worker **>(lua_getextraspace(L))=&worker;
                lua_sethook(L,closeHook,LUA_MASKCOUNT,1000);
                lua_pushcfunction(L,entry);lua_pushlightuserdata(L,&worker);
                const int rc=lua_pcall(L,1,0,0);result.completed=rc==LUA_OK;
                if(rc!=LUA_OK){const char *s=lua_type(L,-1)==LUA_TSTRING?lua_tostring(L,-1):nullptr;result.error=s?s:"non-string Lua error";}
            }
        } else result.error="driver closing";
    } catch (...) {result.completed=false;result.error="worker host exception";}
    worker.closing=true;handoff.close();
    // Teardown is not bytecode execution cancellation: allow Lua finalizers
    // to run, with every dosbox binding explicitly refusing closing effects.
    if(L){lua_sethook(L,nullptr,0,0);lua_close(L);}
    handoff.luaClosed();
}
}

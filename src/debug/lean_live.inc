/* GPL-2.0-or-later. Live Lua projection of the shared native owner.
 * Original-engine operations settle synchronously on the admitted frame.
 * A retained terminal is evidence, never execution authority or a second job.
 */
uint64_t lean_loaded_owner=0,lean_poison_owner=0;
bool lean_fatal_observed=false;
std::string lean_instance,lean_cwd,lean_build;
struct LeanTerminal {
    uint64_t serial=0,start=0,end=0;
    std::string handle;
    LuaDriver::SaveEffects effects;
    bool loaded=false;
};
LeanTerminal lean_terminal;
uint64_t lean_next=0;
void LeanText(lua_State *L,const char *key,const char *value) {
    lua_pushstring(L,value);lua_setfield(L,-2,key);
}
void LeanText(lua_State *L,const char *key,const std::string &value) {
    lua_pushlstring(L,value.data(),value.size());lua_setfield(L,-2,key);
}
void LeanCounter(lua_State *L,const char *key,uint64_t value) {
    char text[32];snprintf(text,sizeof(text),"%llu",(unsigned long long)value);
    LeanText(L,key,text);
}
bool LeanOwned() {
    return Debugger_LiveOwnsStop(DOSBOX_MachineServiceOwner().token) &&
        !strcmp(Debugger_LiveStopReason(),"deliberate");
}
void LeanE(lua_State *L) {
    lua_newtable(L);LeanText(L,"instance",lean_instance);
    LeanCounter(L,"frame",DEBUG_NativeExecutionContext().invocation);
    LeanCounter(L,"generation",DEBUG_NativeExecutionContext().checkpoint);
}
void LeanStartup() {
    char cwd[4097];
    if(!getcwd(cwd,sizeof(cwd)))E_Exit("Lua startup cwd unavailable");
    lean_cwd=cwd;
    unsigned char random[16];int fd=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(fd<0)E_Exit("Lua instance unavailable");
    size_t count=0;
    while(count<sizeof(random)) {
        const ssize_t n=read(fd,random+count,sizeof(random)-count);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){close(fd);E_Exit("Lua instance unavailable");}
        count+=size_t(n);
    }
    close(fd);
    const char *hex="0123456789abcdef";
    for(auto b:random){lean_instance+=hex[b>>4];lean_instance+=hex[b&15];}
    int image=open("/proc/self/exe",O_RDONLY|O_CLOEXEC);
    if(image<0)E_Exit("Lua build identity unavailable");
    CapturePNG::SHA256 hash;uint8_t block[65536];bool ok=true;
    for(;;){const ssize_t n=read(image,block,sizeof(block));
        if(n<0 && errno==EINTR)continue;
        if(n<0){ok=false;break;}if(!n)break;hash.update(block,size_t(n));}
    close(image);if(!ok)E_Exit("Lua build identity read failed");
    char digest[65];hash.finish(digest);lean_build=digest;
    signal(SIGPIPE,SIG_IGN);
}
// No outstanding native borrower exists after a synchronous operation returns.
// Preserve terminal effects across carrier loss, especially failed cleanup.
void LeanSettle(const char *code) {
    if(lean_terminal.serial && !lean_terminal.effects.retired)
        E_Exit("Lua native operation did not settle before %s",code);
}
int LeanError(lua_State *L,const char *code) {
    lua_newtable(L);LeanText(L,"code",code);LeanText(L,"effect","none");
    return lua_error(L);
}
void LeanArguments(lua_State *L) {
    if(lua_gettop(L)!=1 || lua_type(L,1)!=LUA_TTABLE)LeanError(L,"invalid_arguments");
    if(lua_getmetatable(L,1))LeanError(L,"invalid_arguments");
}
void LeanFields(lua_State *L,int table,const char *const *allowed) {
    table=lua_absindex(L,table);
    if(lua_type(L,table)!=LUA_TTABLE || lua_getmetatable(L,table))LeanError(L,"invalid_arguments");
    lua_pushnil(L);
    while(lua_next(L,table)) {
        if(lua_type(L,-2)!=LUA_TSTRING)LeanError(L,"invalid_arguments");
        size_t length=0;const char *name=lua_tolstring(L,-2,&length);bool found=false;
        for(unsigned i=0;allowed[i];++i)
            if(strlen(allowed[i])==length && !memcmp(name,allowed[i],length))found=true;
        if(!found)LeanError(L,"invalid_arguments");
        lua_pop(L,1);
    }
}
void LeanCheckE(lua_State *L) {
    if(lean_poison_owner && lean_poison_owner==DOSBOX_MachineServiceOwner().token)LeanError(L,"poisoned");
    lua_getfield(L,1,"E");
    const char *fields[]={"instance","frame","generation",nullptr};LeanFields(L,-1,fields);
    const auto context=DEBUG_NativeExecutionContext();
    char frame[21],generation[21];
    snprintf(frame,sizeof(frame),"%llu",(unsigned long long)context.invocation);
    snprintf(generation,sizeof(generation),"%llu",(unsigned long long)context.checkpoint);
    const char *values[]={lean_instance.c_str(),frame,generation};bool stale=false;
    for(unsigned i=0;i<3;++i) {
        lua_getfield(L,-1,fields[i]);
        if(lua_type(L,-1)!=LUA_TSTRING)LeanError(L,"invalid_arguments");
        size_t length=0;const char *s=lua_tolstring(L,-1,&length);
        bool valid=i?length>=1 && length<=20 && (length==1 || s[0]!='0'):length==32;
        for(size_t j=0;valid && j<length;++j)
            valid=i?(s[j]>='0' && s[j]<='9'):((s[j]>='0' && s[j]<='9') || (s[j]>='a' && s[j]<='f'));
        if(i && valid && length==20)valid=memcmp(s,"18446744073709551615",20)<=0;
        if(!valid)LeanError(L,"invalid_arguments");
        stale=stale || strlen(values[i])!=length || memcmp(values[i],s,length);
        lua_pop(L,1);
    }
    lua_pop(L,1);if(stale)LeanError(L,"stale_E");
}
int LeanStatus(lua_State *L) {
    LeanArguments(L);const char *fields[]={nullptr};LeanFields(L,1,fields);
    lua_newtable(L);LeanText(L,"owner",LeanOwned()?"owned":"foreign");
    LeanText(L,"run",LeanOwned()?"owned_stopped":Debugger_LiveEffectiveStopped()?"foreign_stopped":"running");
    LeanE(L);lua_setfield(L,-2,"E");
    if(lean_terminal.serial)lua_pushlstring(L,lean_terminal.handle.data(),lean_terminal.handle.size());else lua_pushnil(L);
    lua_setfield(L,-2,"state_handle");
    const auto tasks=GFX_LuaTaskStatus();
    lua_pushboolean(L,NativeControl::owner().busy() || tasks.pending || tasks.held ||
        tasks.cleanup_state==unsigned(NativeInput::CleanupState::Failed));lua_setfield(L,-2,"input_blocked");
    LeanText(L,"entry",entry_hold.lean?(entry_hold.expired?"expired":entry_hold.hit?"held":"armed"):"none");
    if(entry_hold.lean && entry_hold.hit && !entry_hold.expired){lua_pushinteger(L,entry_hold.deadline);lua_setfield(L,-2,"hold_deadline_ms");}
    return 1;
}
int LeanDiscover(lua_State *L) {
    LeanArguments(L);const char *fields[]={nullptr};LeanFields(L,1,fields);
    lua_newtable(L);LeanText(L,"major","automation-1");LeanText(L,"startup_cwd",lean_cwd);
    LeanText(L,"build_id",lean_build);LeanText(L,"profile","original-component-stream");
    lua_newtable(L);lua_setfield(L,-2,"capabilities");
    lua_newtable(L);lua_pushinteger(L,4096);lua_setfield(L,-2,"read_bytes");lua_setfield(L,-2,"limits");return 1;
}
int LeanControl(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E",nullptr};LeanFields(L,1,fields);LeanCheckE(L);
    if(NativeControl::owner().busy())return LeanError(L,"busy");
    const bool resume=lua_toboolean(L,lua_upvalueindex(1));
    if(!DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext(),nullptr,false))return LeanError(L,"wrong_owner");
    if(resume && entry_hold.lean && entry_hold.expired)return LeanError(L,"entry_expired");
    lua_newtable(L);LeanText(L,"outcome","completed");LeanText(L,"effect","complete");
    LeanText(L,"run",resume?"running":"owned_stopped");LeanText(L,"code",resume?"resumed":"stopped");
    LeanE(L);lua_setfield(L,-2,"E");
    if(resume?!Debugger_LiveResume(DOSBOX_MachineServiceOwner().token):!Debugger_LiveStop(DOSBOX_MachineServiceOwner().token))return LeanError(L,"wrong_owner");
    if(resume && entry_hold.lean && entry_hold.hit){entry_hold.load_eligible=false;entry_hold.launch=false;}
    return 1;
}
int LeanEntryArm(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E","program","timeout_ms","hold_ms",nullptr};LeanFields(L,1,fields);LeanCheckE(L);
    lua_getfield(L,1,"timeout_ms");if(!lua_isinteger(L,-1))return LeanError(L,"invalid_arguments");
    const lua_Integer timeout=lua_tointeger(L,-1);lua_pop(L,1);
    lua_getfield(L,1,"hold_ms");if(!lua_isinteger(L,-1))return LeanError(L,"invalid_arguments");
    const lua_Integer hold=lua_tointeger(L,-1);lua_pop(L,1);
    lua_getfield(L,1,"program");size_t length=0;const char *program=lua_tolstring(L,-1,&length);
    if(lua_type(L,-1)!=LUA_TSTRING || memchr(program,0,length) || timeout<1 || timeout>30000 || hold<1 || hold>120000)return LeanError(L,"invalid_arguments");
    const auto owner=DOSBOX_MachineServiceOwner();
    if(!DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext()) || !LeanOwned())return LeanError(L,"wrong_owner");
    if(owner.depth!=1 || owner.kind!=MachineService::Kind::RootGuest || entry_hold.launch || entry_hold.generation ||
       lean_terminal.serial || NativeControl::owner().busy())return LeanError(L,"not_ready");
    const auto tasks=GFX_LuaTaskStatus();if(tasks.pending || tasks.held || PredicateEnabled() || Debugger_PredicateHeld())return LeanError(L,"not_ready");
    lua_newtable(L);LeanText(L,"outcome","completed");LeanText(L,"effect","complete");LeanText(L,"code","armed");
    LeanText(L,"run","owned_stopped");LeanE(L);lua_setfield(L,-2,"E");
    bool valid=false;
    {
        EntryHold prepared;prepared.program.assign(program,length);
        valid=LuaDriver::Options::canonicalProgram(prepared.program);
        if(valid) {
            std::replace(prepared.program.begin()+2,prepared.program.end(),'/','\\');
            prepared.launch=prepared.lean=prepared.armed=true;prepared.owner=owner.token;prepared.generation=1;
            prepared.deadline=LiveNow()+uint64_t(timeout);prepared.hold_ms=uint64_t(hold);
            entry_hold=std::move(prepared);
        }
    }
    if(!valid)return LeanError(L,"invalid_arguments");
    return 1;
}
int LeanReady(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E",nullptr};LeanFields(L,1,fields);LeanCheckE(L);
    lua_newtable(L);
    bool ready=false;char reason[241]={};
    {
        LuaDriver::Response response;
        Debugger_LocalDriverStateReady(live_attachment,DEBUG_NativeExecutionContext(),response);
        ready=response.error.empty() && response.state_reasons.empty() &&
            !NativeControl::owner().busy() && !lean_terminal.serial;
        const char *message=!response.error.empty()?response.error.c_str():
            !response.state_reasons.empty()?response.state_reasons.front().c_str():
            "native admission unavailable";
        snprintf(reason,sizeof(reason),"%s",message);
    }
    lua_pushboolean(L,ready);lua_setfield(L,-2,"ready");lua_newtable(L);
    if(!ready){lua_pushstring(L,reason);lua_rawseti(L,-2,1);}
    lua_setfield(L,-2,"reasons");return 1;
}
int LeanState(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E","action","path","timeout_ms",nullptr};LeanFields(L,1,fields);LeanCheckE(L);
    lua_getfield(L,1,"action");size_t n=0;const char *action=lua_tolstring(L,-1,&n);
    const bool save=lua_type(L,-1)==LUA_TSTRING && n==4 && !memcmp(action,"save",4);
    const bool load=lua_type(L,-1)==LUA_TSTRING && n==4 && !memcmp(action,"load",4);
    lua_pop(L,1);if(!save && !load)return LeanError(L,"unsupported");
    if(lean_terminal.serial || NativeControl::owner().busy())return LeanError(L,"busy");
    if(!DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext()))return LeanError(L,"wrong_owner");
    // Original codecs do not serialize Lua stacks. Restore remains a one-shot
    // entry operation, not arbitrary replay inside an executing script.
    if(load) {
        const auto owner=DOSBOX_MachineServiceOwner();
        if(lean_loaded_owner==owner.token)return LeanError(L,"load_consumed");
        if(!entry_hold.lean || !entry_hold.hit || !entry_hold.load_eligible ||
           entry_hold.expired || entry_hold.owner!=owner.token ||
           LiveNow()>=entry_hold.deadline)return LeanError(L,"entry_required");
    }
    lua_getfield(L,1,"timeout_ms");if(!lua_isinteger(L,-1))return LeanError(L,"invalid_arguments");
    const lua_Integer timeout=lua_tointeger(L,-1);lua_pop(L,1);
    lua_getfield(L,1,"path");size_t length=0;const char *path=lua_tolstring(L,-1,&length);
    if(lua_type(L,-1)!=LUA_TSTRING || !length || length>4096 || memchr(path,0,length) || timeout<1 || timeout>30000 || lean_next==UINT64_MAX)return LeanError(L,"invalid_arguments");
    char handle[96];snprintf(handle,sizeof(handle),"%s:state:%llu",lean_instance.c_str(),(unsigned long long)(lean_next+1));
    lua_newtable(L);LeanText(L,"handle",handle);LeanText(L,"kind","state");
    const uint64_t start=LiveNow();lua_pushinteger(L,start);lua_setfield(L,-2,"host_start_ms");
    lua_pushinteger(L,start+uint64_t(timeout));lua_setfield(L,-2,"native_deadline_ms");
    // Prepare retained evidence and all path allocation before native effects.
    {
        const std::string filename(path,length);
        LeanTerminal prepared;prepared.handle=handle;prepared.start=start;
        prepared.effects.code.reserve(64);
        lean_terminal=std::move(prepared);lean_terminal.serial=++lean_next;
        if(save)Debugger_LocalDriverSave(live_attachment,DEBUG_NativeExecutionContext(),filename,uint32_t(timeout),lean_terminal.effects);
        else {
            const char *error=Debugger_LocalDriverLoad(live_attachment,DEBUG_NativeExecutionContext(),filename);
            lean_terminal.loaded=error==nullptr;
            lean_terminal.effects.valid=true;lean_terminal.effects.completed=!error;
            lean_terminal.effects.retired=true;
            lean_terminal.effects.usable=DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext());
            lean_terminal.effects.code=error?error:"loaded";
            if(!error) {
                lean_loaded_owner=DOSBOX_MachineServiceOwner().token;
                entry_hold.load_eligible=false;entry_hold.launch=false;
            }
        }
        lean_terminal.end=LiveNow();
    }
    return 1; // no Lua allocation after native effects
}
int LeanJobControl(lua_State *L) {
    LeanArguments(L);const char *fields[]={"handle",nullptr};LeanFields(L,1,fields);
    lua_getfield(L,1,"handle");size_t n=0;const char *handle=lua_tolstring(L,-1,&n);
    if(lua_type(L,-1)!=LUA_TSTRING || !lean_terminal.serial || n!=lean_terminal.handle.size() || memcmp(handle,lean_terminal.handle.data(),n))return LeanError(L,"unknown_handle");
    const int op=lua_tointeger(L,lua_upvalueindex(1));
    if(op==2) {
        if(lean_terminal.effects.cleanup_failed)return LeanError(L,"cleanup_failed");
        if(lean_terminal.effects.poisoned)return LeanError(L,"poisoned");
        if(!lean_terminal.effects.retired)return LeanError(L,"pending");
        lua_newtable(L);lua_pushboolean(L,true);lua_setfield(L,-2,"released");
        lean_terminal=LeanTerminal{};return 1;
    }
    lua_newtable(L);LeanText(L,"state","terminal");
    LeanText(L,"outcome",lean_terminal.effects.completed?"completed":"failed");
    LeanText(L,"code",lean_terminal.effects.code);
    LeanText(L,"effect",lean_terminal.effects.completed?"complete":lean_terminal.effects.published?"partial":"none");
    lua_pushinteger(L,lean_terminal.start);lua_setfield(L,-2,"host_start_ms");
    lua_pushinteger(L,lean_terminal.end);lua_setfield(L,-2,"host_end_ms");
    if(lean_terminal.effects.usable && DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext())){LeanE(L);lua_setfield(L,-2,"E");}
    lua_newtable(L);LeanText(L,"guest",lean_terminal.loaded?"loaded_stopped":"unchanged");
    LeanText(L,"file_effect",lean_terminal.effects.published?"published":"none");
    LeanText(L,"entry","not_applicable");
    lua_pushboolean(L,lean_terminal.effects.durable);lua_setfield(L,-2,"durable");
    lua_newtable(L);LeanText(L,"state",lean_terminal.effects.cleanup_attempted?(lean_terminal.effects.cleanup_failed?"failed":"removed"):"none");
    lua_setfield(L,-2,"cleanup");lua_setfield(L,-2,"data");return 1;
}
void LeanReadAdmission(lua_State *L) {
    LeanCheckE(L);if(NativeControl::owner().busy())LeanError(L,"busy");
    if(!DEBUG_AdmitNativeState(live_attachment,DEBUG_NativeExecutionContext()) || !LeanOwned())LeanError(L,"wrong_owner");
}
int LeanRegisters(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E","names",nullptr};LeanFields(L,1,fields);LeanReadAdmission(L);
    static const char *names[]={"EAX","EBX","ECX","EDX","ESI","EDI","EBP","ESP","EIP","EFLAGS","CS","DS","ES","SS","FS","GS"};
    unsigned selected[16],count=0;uint32_t seen=0;
    lua_getfield(L,1,"names");
    if(lua_type(L,-1)!=LUA_TTABLE || lua_getmetatable(L,-1))return LeanError(L,"invalid_arguments");
    const int table=lua_absindex(L,-1);const size_t n=lua_rawlen(L,table);
    if(n<1 || n>16)return LeanError(L,"invalid_arguments");
    lua_pushnil(L);while(lua_next(L,table)) {
        if(!lua_isinteger(L,-2) || lua_tointeger(L,-2)<1 || uint64_t(lua_tointeger(L,-2))>n)return LeanError(L,"invalid_arguments");
        ++count;lua_pop(L,1);
    }
    if(count!=n)return LeanError(L,"invalid_arguments");
    for(unsigned i=0;i<count;++i){lua_rawgeti(L,table,i+1);size_t length=0;
        if(lua_type(L,-1)!=LUA_TSTRING)return LeanError(L,"invalid_arguments");
        const char *name=lua_tolstring(L,-1,&length);unsigned index=16;
        for(unsigned j=0;j<16;++j)if(strlen(names[j])==length && !memcmp(name,names[j],length))index=j;
        if(index==16 || (seen&(1u<<index)))return LeanError(L,"invalid_arguments");
        seen|=1u<<index;selected[i]=index;lua_pop(L,1);
    }
    const uint32_t values[]={reg_eax,reg_ebx,reg_ecx,reg_edx,reg_esi,reg_edi,reg_ebp,reg_esp,reg_eip,CPU_InspectFlags(),SegValue(cs),SegValue(ds),SegValue(es),SegValue(ss),SegValue(fs),SegValue(gs)};
    lua_newtable(L);LeanE(L);lua_setfield(L,-2,"E");LeanCounter(L,"guest_tick",PIC_Ticks);
    lua_newtable(L);for(unsigned i=0;i<count;++i){lua_newtable(L);LeanText(L,"name",names[selected[i]]);lua_pushinteger(L,values[selected[i]]);lua_setfield(L,-2,"value");lua_rawseti(L,-2,i+1);}
    lua_setfield(L,-2,"values");return 1;
}
int LeanReadMemory(lua_State *L) {
    LeanArguments(L);const char *fields[]={"E","address","length",nullptr};LeanFields(L,1,fields);LeanReadAdmission(L);
    lua_getfield(L,1,"address");if(!lua_isinteger(L,-1))return LeanError(L,"invalid_arguments");const lua_Integer address=lua_tointeger(L,-1);lua_pop(L,1);
    lua_getfield(L,1,"length");if(!lua_isinteger(L,-1))return LeanError(L,"invalid_arguments");const lua_Integer length=lua_tointeger(L,-1);lua_pop(L,1);
    if(address<0 || uint64_t(address)>UINT32_MAX || length<1 || length>4096 || uint64_t(address)+uint64_t(length)>16u*1024u*1024u)return LeanError(L,"invalid_arguments");
    uint8_t bytes[4096];char hex[8192];if(!MEM_InspectPhysicalRam(uint32_t(address),uint32_t(length),bytes))return LeanError(L,"not_ram");
    const char *digits="0123456789abcdef";
    for(unsigned i=0;i<unsigned(length);++i){hex[2*i]=digits[bytes[i]>>4];hex[2*i+1]=digits[bytes[i]&15];}
    lua_newtable(L);LeanE(L);lua_setfield(L,-2,"E");LeanCounter(L,"guest_tick",PIC_Ticks);lua_pushinteger(L,address);lua_setfield(L,-2,"address");lua_pushlstring(L,hex,size_t(length)*2);lua_setfield(L,-2,"bytes");return 1;
}
int LeanUnsupported(lua_State *L) {LeanArguments(L);return LeanError(L,"unsupported");}
void LeanRegister(lua_State *L) {
    lua_newtable(L);
    const struct {const char *name;lua_CFunction fn;} functions[]={
        {"registers",LeanRegisters},{"read_memory",LeanReadMemory},{"discover",LeanDiscover},{"status",LeanStatus},{"entry_arm",LeanEntryArm},{"state_ready",LeanReady},{"state_file",LeanState}};
    for(auto f:functions){lua_pushcfunction(L,f.fn);lua_setfield(L,-2,f.name);}
    for(int i=0;i<2;++i){lua_pushboolean(L,i);lua_pushcclosure(L,LeanControl,1);lua_setfield(L,-2,i?"resume":"stop");}
    const char *names[]={"poll","cancel","release"};for(int i=0;i<3;++i){lua_pushinteger(L,i);lua_pushcclosure(L,LeanJobControl,1);lua_setfield(L,-2,names[i]);}
    const char *unsupported[]={"input_sequence","input_edges","observe_frame","target_arm","run_snapshot","pointer_geometry","pointer_observation","close",nullptr};
    for(unsigned i=0;unsupported[i];++i){lua_pushcfunction(L,LeanUnsupported);lua_setfield(L,-2,unsupported[i]);}
    lua_setglobal(L,"automation");
}

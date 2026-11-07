/* GPL-2.0-or-later. Borrowed endpoint/worker ownership, not emulator proof. */
#include "lua_driver_worker.h"
#include "lua_session_transport.h"
#include <cassert>
#include <thread>
#include <cstdlib>
#include <string>
using namespace LuaDriver;
static std::string line(int fd) {
    std::string out;char ch;
    while(recv(fd,&ch,1,0)==1){out+=ch;if(ch=='\n')break;}return out;
}
int main() {
    setenv("LUA_REPL_MCP_SESSION_FD","3",1);
    setenv("LUA_REPL_MCP_ALLOCATION_GENERATION","1",1);
    setenv("LUA_REPL_MCP_LIMITS","{\"input_bytes\":1048576,\"values_bytes\":1048576,\"diagnostic_bytes\":65536,\"max_depth\":64,\"worker_bytes\":4194304,\"response_bytes\":16777216,\"output_bytes\":33554432}",1);
    for(int mode=0;mode<4;++mode) {
        fprintf(stderr,"mode %d start\n",mode);
        int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
        if(pair[0]!=3){assert(dup2(pair[0],3)==3);close(pair[0]);}
        assert(!sessionEndpoint().adopt());
        assert(fcntl(3,F_GETFD)&FD_CLOEXEC);
        Handoff handoff;WorkerResult result;Options options;
        assert(options.select("lua-driver-session",""));
        std::thread worker(runWorker,std::cref(options),std::ref(handoff),std::ref(result));
        assert(handoff.ready());fprintf(stderr,"waiting ready fd=%d peer=%d\n",3,pair[1]);assert(line(pair[1]).find("ready")!=std::string::npos);fprintf(stderr,"ready\n");
        if(mode==1 || mode==2) {
            const char *code=mode==1?"return require('dosbox').step().reason":"local d=require('dosbox'); local b=d.breakpoint(1,2,function(e) return true end); return d.run()";
            std::string record="{\"type\":\"execute\",\"protocol_version\":1,\"allocation_generation\":1,\"execution_generation\":1,\"input\":{\"code\":\""+std::string(code)+"\"}}\n";
            assert(send(pair[1],record.data(),record.size(),MSG_NOSIGNAL)==ssize_t(record.size()));
            Request request;
            while(!handoff.take(request))std::this_thread::yield();
            if(mode==2) {
                assert(request.operation==Operation::Breakpoint);
                Response registered;registered.registration=1;handoff.complete(std::move(registered));
                while(!handoff.take(request))std::this_thread::yield();
                assert(request.operation==Operation::Run);
                Event event;event.registration=1;event.ordinal=1;
                assert(handoff.offerEvent(std::move(event)));
                Verdict verdict;
                while(!handoff.takeVerdict(verdict))std::this_thread::yield();
            }
            handoff.close();sessionEndpoint().wake();
            Response response;response.error="external lifecycle closed";
            assert(handoff.complete(std::move(response)));
        } else if(mode==0) {handoff.close();sessionEndpoint().wake();}
        else {shutdown(pair[1],SHUT_RDWR);}
        worker.join();assert(handoff.isLuaClosed());
        assert(fcntl(3,F_GETFD)>=0); // borrowed FD remains owned through Lua close
        sessionEndpoint().release();assert(fcntl(3,F_GETFD)<0);
        close(pair[1]);
    }
}

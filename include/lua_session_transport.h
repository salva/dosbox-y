/* GPL-2.0-or-later. Startup-owned endpoint; worker borrows until joined. */
#ifndef DOSBOX_LUA_SESSION_TRANSPORT_H
#define DOSBOX_LUA_SESSION_TRANSPORT_H
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace LuaDriver {
class SessionEndpoint {
    int fd_=-1;
public:
    SessionEndpoint()=default;
    SessionEndpoint(const SessionEndpoint &)=delete;
    SessionEndpoint &operator=(const SessionEndpoint &)=delete;
    ~SessionEndpoint() { release(); }
    const char *adopt() {
        if(fd_>=0)return "session endpoint already adopted";
        // Ownership transfers only for the explicitly selected FD3 ABI.
        fd_=3;
        const int flags=fcntl(fd_,F_GETFD);
        if(flags<0 || fcntl(fd_,F_SETFD,flags|FD_CLOEXEC)<0)
            return "cannot mark session FD3 close-on-exec";
        int type=0;socklen_t size=sizeof(type);
        if(getsockopt(fd_,SOL_SOCKET,SO_TYPE,&type,&size)<0 || type!=SOCK_STREAM)
            return "session FD3 must be a stream socket";
        const int status=fcntl(fd_,F_GETFL);
        if(status<0 || (status&O_NONBLOCK))return "session FD3 must be blocking";
        return nullptr;
    }
    int borrowed() const {return fd_;}
    // Native owner only; does not close/reuse the worker's borrowed descriptor.
    void wake() {if(fd_>=0)shutdown(fd_,SHUT_RDWR);}
    // Native owner only, after worker join (or startup failed before spawn).
    void release() {if(fd_>=0){const int fd=fd_;fd_=-1;::close(fd);}}
};
inline SessionEndpoint &sessionEndpoint() {static SessionEndpoint endpoint;return endpoint;}
}
#endif

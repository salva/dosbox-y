#!/usr/bin/env python3
"""Actual POSIX transport resource/framing regression, no guest authority claim."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
work = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'unix-transport'
work.mkdir(exist_ok=True)
(work/'config.h').write_text('#define C_DEBUG 1\n#define C_DOSBOX_AGENT 1\n')
source = r'''
#include "config.h"
#include "rpc_transport.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <atomic>
#include <dirent.h>
#include <fcntl.h>
using namespace dosbox_agent;
int connect_to(const std::string& path) {
 int fd=socket(AF_UNIX,SOCK_STREAM,0); assert(fd>=0);
 sockaddr_un a={};a.sun_family=AF_UNIX;strcpy(a.sun_path,path.c_str());
 assert(connect(fd,(sockaddr*)&a,sizeof(a))==0);return fd;
}
int main(int argc,char** argv) {
 assert(argc==2);std::string dir=argv[1];assert(mkdir(dir.c_str(),0700)==0);
 AgentConfig c;c.endpoint=dir+"/rpc.sock";c.max_message_bytes=4;
 auto t=AGENT_CreateLocalTransport(AgentTransport::UnixSocket);
 std::string error;
 std::atomic<unsigned> failures{0};
 auto echo=[&](const std::string& s){if(s=="fail"){++failures;throw std::runtime_error("uncertain");}return s;};
 // Existing path is never removed, even on disposal.
 {std::ofstream f(c.endpoint);f<<"retain";}
 assert(!t->Start(c,echo,&error));t->Stop();
 struct stat st={};assert(lstat(c.endpoint.c_str(),&st)==0&&S_ISREG(st.st_mode));
 assert(unlink(c.endpoint.c_str())==0);
 assert(t->Start(c,echo,&error));assert(!t->Start(c,echo,&error));
 assert(lstat(c.endpoint.c_str(),&st)==0&&S_ISSOCK(st.st_mode)&&(st.st_mode&0777)==0600);
 int fd=connect_to(c.endpoint);
 assert(send(fd,"ab",2,MSG_NOSIGNAL)==2);assert(send(fd,"cd\nxy\r\n",7,MSG_NOSIGNAL)==7);
 char b[32]={};std::string answer;
 while(answer.size()<8){auto n=recv(fd,b,sizeof(b),0);assert(n>0);answer.append(b,n);}
 assert(answer=="abcd\nxy\n");
 // Production listener and accepted socket must not survive worker exec.
 // This isolated process has exactly one client socket of its own.
 DIR* descriptors=opendir("/proc/self/fd");assert(descriptors);
 unsigned owned_sockets=0;
 while(auto entry=readdir(descriptors)) {
   char* end=nullptr;long number=strtol(entry->d_name,&end,10);
   if(!*entry->d_name || *end || number==fd)continue;
   struct stat info={};
   sockaddr_un bound={};socklen_t bound_size=sizeof(bound);
   if(fstat(int(number),&info)==0 && S_ISSOCK(info.st_mode) &&
      getsockname(int(number),(sockaddr*)&bound,&bound_size)==0 &&
      bound.sun_family==AF_UNIX && bound.sun_path==c.endpoint) {
     assert(fcntl(int(number),F_GETFD)&FD_CLOEXEC);++owned_sockets;
   }
 }
 closedir(descriptors);assert(owned_sockets==2);
 // Idle connected client must not strand Stop.
 t->Stop();close(fd);assert(lstat(c.endpoint.c_str(),&st)<0);t->Stop();
 assert(t->Start(c,echo,&error));fd=connect_to(c.endpoint);
 assert(send(fd,"12345",5,MSG_NOSIGNAL)==5);assert(recv(fd,b,sizeof(b),0)==0);close(fd);
 fd=connect_to(c.endpoint);assert(send(fd,"fail\n",5,MSG_NOSIGNAL)==5);
 assert(recv(fd,b,sizeof(b),0)==0);close(fd);
 // Worker survives failed connection; handler is not replayed.
 fd=connect_to(c.endpoint);assert(send(fd,"ok\n",3,MSG_NOSIGNAL)==3);
 assert(recv(fd,b,sizeof(b),0)==3);close(fd);
 // Replaced path is not ours to unlink.
 assert(unlink(c.endpoint.c_str())==0);{std::ofstream f(c.endpoint);f<<"replacement";}
 t->Stop();assert(failures==1);assert(lstat(c.endpoint.c_str(),&st)==0&&S_ISREG(st.st_mode));
 assert(unlink(c.endpoint.c_str())==0);assert(rmdir(dir.c_str())==0);
}
'''
(work/'test.cpp').write_text(source)
subprocess.run(['g++','-std=c++14','-Wall','-Wextra','-Werror','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie', '-I'+str(work),'-I'+str(root/'include'),'-I'+str(root/'src/agent/transport'),str(work/'test.cpp'),str(root/'src/agent/transport/local_transport.cpp'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test'),str(work/'endpoint')],check=True,timeout=15)
print('Actual Unix transport framing/refusal/stop/restart/endpoint ownership PASS; no guest/session qualification')

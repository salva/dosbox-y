#!/usr/bin/env python3
"""Real directory descriptor lifetime for the native capture task owner."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
area = Path(os.environ['SAIVAGE_CARD_WORK_ROOT']) / 'lua-task-root'
area.mkdir(exist_ok=True)
source = r'''
#include "live_tasks.h"
#include <cassert>
#include <dirent.h>
#include <sys/stat.h>
static int retained(dev_t device, ino_t inode) {
    DIR *directory=opendir("/proc/self/fd");assert(directory);
    int found=-1;
    while(auto *entry=readdir(directory)) {
        char *end=nullptr;long fd=strtol(entry->d_name,&end,10);
        if(!*entry->d_name || *end || fd==dirfd(directory))continue;
        struct stat st={};
        if(!fstat(fd,&st) && st.st_dev==device && st.st_ino==inode) {
            assert(found<0);found=fd;
        }
    }
    closedir(directory);return found;
}
int main() {
    mkdir("owned",0700);mkdir("other",0700);
    int fd=open("owned",O_RDONLY|O_DIRECTORY);assert(fd>=0);
    struct stat identity={};assert(!fstat(fd,&identity));
    int held=-1;
    {
        NativeInput::Tasks tasks(fd,1024);assert(!tasks.closed());
        close(fd);
        int other=open("other",O_RDONLY|O_DIRECTORY);assert(other==fd);
        held=retained(identity.st_dev,identity.st_ino);assert(held>=0 && held!=other);
        assert(fcntl(held,F_GETFD)&FD_CLOEXEC);
        close(other);
    }
    assert(fcntl(held,F_GETFD)==-1 && errno==EBADF);
    fd=open("owned",O_RDONLY|O_DIRECTORY);assert(fd>=0);
    {
        NativeInput::Tasks tasks(fd,1024);close(fd);
        held=retained(identity.st_dev,identity.st_ino);assert(held>=0);
        tasks.channel_fault();assert(tasks.closed());
        assert(fcntl(held,F_GETFD)==-1 && errno==EBADF);
        int other=open("other",O_RDONLY|O_DIRECTORY);assert(other>=0);
        assert(dup2(other,held)==held);
        tasks.channel_fault(); // must not close a reused descriptor number
        assert(fcntl(held,F_GETFD)>=0);
        if(other!=held)close(other);
    }
    assert(fcntl(held,F_GETFD)>=0);close(held);
    NativeInput::Tasks invalid(-1,1024);assert(invalid.closed());
    puts("native task directory ownership PASS (not production integration)");
}
'''
(area / 'test.cpp').write_text(source)
subprocess.run(['c++', '-std=c++14', '-Wall', '-Wextra', '-Werror',
                '-I'+str(root/'include'), '-I'+str(root/'src/hardware'),
                str(area/'test.cpp'), '-o', str(area/'test')], check=True)
subprocess.run([str(area/'test')], cwd=area, check=True)

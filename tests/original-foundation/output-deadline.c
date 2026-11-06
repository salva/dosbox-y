/* Test-only cooperative deadline witness: delay the owned archive's final fsync.
 * No error injection, other streams untouched. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
int fsync(int fd) {
    int (*real_sync)(int)=dlsym(RTLD_NEXT,"fsync");
    char proc[64],path[4096];
    snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
    ssize_t n=readlink(proc,path,sizeof(path)-1);
    const char *target=getenv("DOSBOX_TEST_DELAY_OUTPUT");
    if(n>=0){path[n]=0;if(target && !strncmp(path,target,strlen(target)) &&
        !strncmp(path+strlen(target),".tmp-",5)) {
            const char *marker=getenv("DOSBOX_TEST_DELAY_MARKER");
            if(marker){FILE *f=fopen(marker,"w");if(f){fputs("final fsync entered",f);fclose(f);}}
            usleep(300000);
        }}
    return real_sync(fd);
}

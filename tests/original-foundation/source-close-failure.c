/* Failure witness only: close the exact read-only source successfully, then
 * report its native close failure. Never alters source bytes or other streams.
 * Build as an LD_PRELOAD shared object on Linux; not production recovery code.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
int fclose(FILE *stream) {
    int (*close_real)(FILE *) = dlsym(RTLD_NEXT, "fclose");
    char proc[64], path[4096];
    const char *target=getenv("DOSBOX_TEST_CLOSE_SOURCE");
    snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fileno(stream));
    const ssize_t n=readlink(proc,path,sizeof(path)-1);
    if(n>=0) path[n]=0;
    const int selected=target && n>=0 && !strcmp(target,path);
    const int result=close_real(stream);
    if(selected) {errno=EIO; return EOF;}
    return result;
}

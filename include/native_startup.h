/* GPL-2.0-or-later. Narrow inherited-FD startup, no host pathname opener. */
#ifndef DOSBOX_NATIVE_STARTUP_H
#define DOSBOX_NATIVE_STARTUP_H
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace NativeInput {
struct Startup {
    int input=-1,output=-1,root=-1;
    size_t limit=0;
    char instance[65]={};
};
inline bool decimal(const char *text,uint64_t maximum,uint64_t &value) {
    value=0;if(!text||!*text)return false;
    for(size_t i=0;text[i];i++) {
        if(i>=20||text[i]<'0'||text[i]>'9')return false;
        unsigned digit=unsigned(text[i]-'0');
        if(value>(maximum-digit)/10 || digit>maximum)return false;
        value=value*10+digit;
    }
    return value<=maximum;
}
inline bool startup(const char *input,const char *output,const char *root,
                    const char *limit,const char *instance,Startup &result) {
    result=Startup();uint64_t in,out,dir,bytes;
    if(!decimal(input,INT_MAX,in)||!decimal(output,INT_MAX,out)||!decimal(root,INT_MAX,dir)||
        !decimal(limit,16u*1024u*1024u,bytes)||!bytes||in==out||in==dir||out==dir||!instance)return false;
    size_t n=strlen(instance);if(n<1||n>64)return false;
    for(size_t i=0;i<n;i++) {
        char c=instance[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
            (c>='0'&&c<='9')||c=='_'||c=='-'))return false;
    }
    struct stat a,b,c;
    int inf=fcntl(int(in),F_GETFL),outf=fcntl(int(out),F_GETFL),dirf=fcntl(int(dir),F_GETFL);
    if(inf<0||outf<0||dirf<0||!(inf&O_NONBLOCK)||!(outf&O_NONBLOCK)||
        (inf&O_ACCMODE)!=O_RDONLY||(outf&O_ACCMODE)!=O_WRONLY||
        fstat(int(in),&a)||fstat(int(out),&b)||fstat(int(dir),&c)||
        !S_ISFIFO(a.st_mode)||!S_ISFIFO(b.st_mode)||!S_ISDIR(c.st_mode)||
        c.st_uid!=geteuid()||(c.st_mode&0077)||
        (a.st_dev==b.st_dev && a.st_ino==b.st_ino))return false;
    result.input=int(in);result.output=int(out);result.root=int(dir);result.limit=size_t(bytes);
    memcpy(result.instance,instance,n+1);return true;
}
}
#endif

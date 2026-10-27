/* GPL-2.0-or-later. POSIX scoped capture publisher, no path-taking command API. */
#ifndef DOSBOX_CAPTURE_PUBLISH_H
#define DOSBOX_CAPTURE_PUBLISH_H
#include "capture_png.h"
#include "capture_sha256.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
namespace CapturePNG {
struct Publication {
    bool completed = false;
    bool quarantined = false;
    size_t bytes = 0;
    uint32_t width = 0, height = 0;
    char basename[64] = {};
    char quarantine_basename[64] = {};
    char sha256[65] = {};
};
struct FileIO {
    int (*unlink)(int, const char *, int) = unlinkat;
    ssize_t (*read)(int, void *, size_t) = ::read;
    int (*close)(int) = ::close;
};
// Root fd is already admitted/owned by caller. This layer does not acquire,
// retain, duplicate, reset or authorize roots or quotas. No request pathname.
inline Publication publish(int root, uint64_t sequence, uint64_t frame,
        uint32_t width, uint32_t height, uint32_t bpp, size_t pitch,
        bool dblw, bool dblh, const uint8_t *pixels, const uint8_t *palette,
        size_t limit, IO io = IO(), FileIO fileio = FileIO()) {
    Publication result;
    struct stat st;
    if (!sequence || !frame || fstat(root,&st)!=0 || !S_ISDIR(st.st_mode)) return result;
    char name[64];
    int n=snprintf(name,sizeof(name),"capture-%llu-%llu.png",
        static_cast<unsigned long long>(sequence),static_cast<unsigned long long>(frame));
    if (n<=0 || size_t(n)>=sizeof(name)) return result;
    int fd=openat(root,name,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if (fd<0) return result; // existing names/symlinks never touched
    // Independent read descriptor observes actual file bytes after writer close.
    int readfd=fcntl(fd,F_DUPFD_CLOEXEC,0);
    FILE *file=fdopen(fd,"wb");
    bool ok=false;
    size_t encoded=0;
    if (file) {
        if (readfd>=0) ok=write(file,width,height,bpp,pitch,dblw,dblh,
            pixels,palette,false,"DOSBox-Y native capture",limit,io,&encoded);
        else fclose(file);
    } else close(fd);
    char hash[65] = {};
    if (ok && readfd>=0) {
        SHA256 digest;
        size_t total=0;
        uint8_t block[8192];
        if (fstat(readfd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0 ||
            uint64_t(st.st_size)!=encoded || encoded>limit || lseek(readfd,0,SEEK_SET)<0) ok=false;
        unsigned interruptions=0;
        while (ok && total<encoded) {
            size_t want=encoded-total < sizeof(block) ? encoded-total : sizeof(block);
            ssize_t count=fileio.read(readfd,block,want);
            if (count<0 && errno==EINTR && ++interruptions<=4) continue;
            if (count<=0) {ok=false;break;}
            digest.update(block,size_t(count));total+=size_t(count);
        }
        if (ok) digest.finish(hash);
    }
    if (readfd>=0 && fileio.close(readfd)!=0) ok=false;
    if (!ok) {
        result.quarantined=fileio.unlink(root,name,0)!=0;
        if(result.quarantined)memcpy(result.quarantine_basename,name,size_t(n)+1);
        return result; // never export path/hash/bytes on any failure
    }
    result.completed=true; result.bytes=encoded;
    result.width=width*(dblw?2u:1u);result.height=height*(dblh?2u:1u);
    memcpy(result.basename,name,size_t(n)+1);memcpy(result.sha256,hash,65);
    return result;
}
}
#endif

/* Native minizip settlement regression, no emulator mock or archive format
 * surrogate. Build/link against the project's minizip and zlib sources. */
#include <assert.h>
#include <stdio.h>
#include "vs/zlib/contrib/minizip/unzip.h"
#include "vs/zlib/contrib/minizip/ioapi.h"
static int closes;
static int fail_close(voidpf unused, voidpf stream) {
    (void)unused;
    ++closes;
    assert(fclose((FILE *)stream)==0);
    return -1;
}
int main(int argc,char **argv) {
    assert(argc==2);
    zlib_filefunc64_def io;
    fill_fopen64_filefunc(&io);
    io.zclose_file=fail_close;
    unzFile file=unzOpen2_64(argv[1],&io);
    assert(file);
    closes=0;
    assert(unzClose(file)==UNZ_ERRNO);
    assert(closes==1);
    /* This member has deliberately incorrect CRC but valid stored bytes.
       EOF consumption leaves member settlement for unzClose itself. */
    file=unzOpen2_64(argv[1],&io);
    assert(file && unzLocateFile(file,"bad",1)==UNZ_OK);
    assert(unzOpenCurrentFile(file)==UNZ_OK);
    char bytes[8];
    assert(unzReadCurrentFile(file,bytes,sizeof(bytes))==3);
    closes=0;
    assert(unzClose(file)==UNZ_CRCERROR);
    assert(closes==1);
    puts("native close error and pending CRC preserved; exact stream closed once");
    return 0;
}

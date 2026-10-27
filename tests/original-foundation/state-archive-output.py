#!/usr/bin/env python3
"""Actual retained ZIP output: failure preserves destination; no reopen/replay."""
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[2]
w = Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])/'state-archive-output'
w.mkdir(exist_ok=True)
(w/'test.cpp').write_text(r'''
#include <cassert>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <fcntl.h>
static bool fail_sync = false;
static bool fail_fdopen = false;
static FILE *checked_fdopen(int fd, const char *mode) {
 assert(fcntl(fd,F_GETFD)&FD_CLOEXEC);
 return fail_fdopen ? nullptr : fdopen(fd,mode);
}
static int checked_sync(int fd) { return fail_sync ? -1 : fsync(fd); }
#define fdopen checked_fdopen
#define fsync checked_sync
#include "src/misc/state_archive_output.h"
#undef fsync
#undef fdopen
#include "vs/zlib/contrib/minizip/zip.c"
#include "zipcppstdbuf.h"
static std::string contents(const std::string &p) {
 std::ifstream f(p); return std::string(std::istreambuf_iterator<char>(f),{});
}
static uLong ZCALLBACK reject(voidpf, voidpf, const void*, uLong) {return 0;}
int main(int argc,char **argv) {
 assert(argc==2); const std::string path=argv[1];
 {std::ofstream f(path);f<<"old";}
 for(int mode=0;mode<4;++mode) {
  StateArchiveOutput out(path); zlib_filefunc64_def io;
  assert(out.prepare(io)); assert(!out.prepare(io)); assert(!out.publish());
  if(mode==0) continue; // reservation discarded, prior destination retained
  if(mode==1)io.zwrite_file=reject;
  zipFile z=zipOpen2_64(path.c_str(),APPEND_STATUS_CREATE,nullptr,&io);assert(z);
  assert(!io.zopen64_file(io.opaque,path.c_str(),ZLIB_FILEFUNC_MODE_CREATE));
  if(mode==3) {
   // Force publication failure after complete close, without removing output.
   assert(std::remove(path.c_str())==0);assert(mkdir(path.c_str(),0700)==0);
  }
  const int rc=zipClose(z,nullptr);
  if(mode==1){assert(rc!=ZIP_OK);assert(contents(path)=="old");continue;}
  assert(rc==ZIP_OK);
  if(mode==3){assert(!out.publish());assert(rmdir(path.c_str())==0);continue;}
  assert(contents(path)=="old");assert(out.publish());assert(!out.publish());
  assert(contents(path)!="old");
 }
 // A sync failure is consumed once, leaves the old destination untouched,
 // and must never authorize publication even after the caller retries it.
 {std::ofstream f(path);f<<"old";}
 {StateArchiveOutput out(path);zlib_filefunc64_def io;assert(out.prepare(io));
  zipFile z=zipOpen2_64(path.c_str(),APPEND_STATUS_CREATE,nullptr,&io);assert(z);
  fail_sync=true;assert(zipClose(z,nullptr)!=ZIP_OK);fail_sync=false;
  assert(!out.publish());assert(contents(path)=="old");}
 // A serializer exception closes the ZIP borrower before output cleanup.
 StateArchive::Effects unwound;
 try {
  StateArchiveOutput out(path,&unwound);zipFile z=nullptr;StateArchiveZip archive(z);
  zlib_filefunc64_def io;assert(out.prepare(io));
  z=zipOpen2_64(path.c_str(),APPEND_STATUS_CREATE,nullptr,&io);assert(z);
  zip_fileinfo info={};
  assert(zipOpenNewFileInZip(z,"component",&info,nullptr,0,nullptr,0,nullptr,0,0)==ZIP_OK);
  zip_ostreambuf buffer(z);std::ostream component(&buffer);
  component<<"partial owner payload";
  throw 7;
 } catch(int value) {assert(value==7);}
 assert(unwound.cleanup_attempted && !unwound.cleanup_failed && !unwound.published);
 assert(contents(path)=="old");
 // FILE conversion failure still retires the reserved descriptor/path.
 StateArchive::Effects conversion;
 {StateArchiveOutput out(path,&conversion);zlib_filefunc64_def io;
  fail_fdopen=true;assert(!out.prepare(io));fail_fdopen=false;
  assert(!out.publish());assert(contents(path)=="old");}
 assert(conversion.cleanup_attempted && !conversion.cleanup_failed && !conversion.published);
 // Effect reporting is consumed only after destructor cleanup has settled.
 StateArchive::Effects abandoned;
 {StateArchiveOutput out(path,&abandoned);zlib_filefunc64_def io;
  assert(out.prepare(io));assert(!abandoned.published);}
 assert(abandoned.cleanup_attempted && !abandoned.cleanup_failed);
 StateArchive::Effects published;
 {StateArchiveOutput out(path,&published);zlib_filefunc64_def io;
  assert(out.prepare(io));zipFile z=zipOpen2_64(path.c_str(),APPEND_STATUS_CREATE,nullptr,&io);
  assert(z);assert(zipClose(z,nullptr)==ZIP_OK);assert(out.publish());}
 assert(published.published && !published.cleanup_attempted && !published.cleanup_failed);
 // A retained-output cleanup failure is reported, not silently retired.
 StateArchive::Effects failed_cleanup;
 std::string reserved;
 {StateArchiveOutput out(path,&failed_cleanup);zlib_filefunc64_def io;
  assert(out.prepare(io));glob_t matches={};
  assert(glob((path+".tmp-*").c_str(),0,nullptr,&matches)==0);
  assert(matches.gl_pathc==1);reserved=matches.gl_pathv[0];globfree(&matches);
  // Replace our own temporary name with a nonempty directory to inject the
  // remove failure, without touching the retained destination.
  assert(unlink(reserved.c_str())==0);assert(mkdir(reserved.c_str(),0700)==0);
  {std::ofstream f(reserved+"/owned");f<<"test";}}
 assert(failed_cleanup.cleanup_attempted && failed_cleanup.cleanup_failed);
 assert(!failed_cleanup.published);
 assert(unlink((reserved+"/owned").c_str())==0);assert(rmdir(reserved.c_str())==0);
 // Repeated completed saves use independently owned outputs.
 for(int i=0;i<3;++i){StateArchiveOutput out(path);zlib_filefunc64_def io;
  assert(out.prepare(io));zipFile z=zipOpen2_64(path.c_str(),APPEND_STATUS_CREATE,nullptr,&io);
  assert(z);assert(zipClose(z,nullptr)==ZIP_OK);assert(out.publish());}
}
''')
subprocess.run(['c++','-std=c++14','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(root),'-I'+str(root/'include'),'-I'+str(root/'vs/zlib/contrib/minizip'),str(w/'test.cpp'),str(root/'vs/zlib/contrib/minizip/ioapi.c'),'-lz','-o',str(w/'test')],check=True)
subprocess.run([str(w/'test'),str(w/'archive.zip')],check=True)
assert not list(w.glob('archive.zip.tmp-*'))
print('retained native ZIP output, abandoned/failed write, failed publication, repeated publication and cleanup: ASan/UBSan PASS')

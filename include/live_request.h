/* GPL-2.0-or-later. Transport-free internal live task request. */
#ifndef DOSBOX_LIVE_REQUEST_H
#define DOSBOX_LIVE_REQUEST_H
#include "native_queue.h"
#include <string.h>
namespace NativeInput {
    inline bool task_identifier(const char *s){if(!s)return false;size_t n=strlen(s);if(n<1||n>64)return false;for(size_t i=0;i<n;i++){char c=s[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;}return true;}
enum class Operation { None, Input, Capture, Poll, Cancel, Ack, Release };
struct Request {
    char id[65]={},target[65]={};
    Operation op=Operation::None;
    Event events[16];
    unsigned count=0;
    uint32_t deadline_ms=0;
};
} // namespace NativeInput
#endif

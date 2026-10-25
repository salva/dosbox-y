/* GPL-2.0-or-later. Dedicated CLI selection, independent of guest startup. */
#ifndef DOSBOX_LUA_DRIVER_OPTIONS_H
#define DOSBOX_LUA_DRIVER_OPTIONS_H
#include <string>
#include <utility>
namespace LuaDriver {
struct Options {
    enum class Script { None, File, Code };
    Script script=Script::None;
    std::string source, program, archive;
    bool program_set=false, archive_set=false;
    // Caller consumes exactly one argv value before passing it here. Presence
    // is separate from content: an empty inline chunk is a valid script.
    bool select(const std::string &name,const std::string &value) {
        if(name=="lua-driver-file" || name=="lua-driver-code") {
            if(script!=Script::None)return false;
            script=name=="lua-driver-file"?Script::File:Script::Code;
            source=value;return true;
        }
        if(name=="lua-driver-program") {
            if(program_set)return false;
            program_set=true;program=value;return true;
        }
        if(name=="lua-driver-state") {
            if(archive_set)return false;
            archive_set=true;archive=value;return true;
        }
        return false;
    }
    // Parse only owned switches, leaving all other argv entries for existing
    // startup parsing. A value is exactly one argument, even if dash-prefixed.
    // Failure leaves both selection and caller cursor unchanged.
    static bool isSwitch(const std::string &arg) {
        return arg=="-lua-driver-file" || arg=="-lua-driver-code" ||
               arg=="-lua-driver-program" || arg=="-lua-driver-state";
    }
    bool consume(int argc,const char *const *argv,int &cursor,std::string &error) {
        if(cursor<0 || cursor>=argc || !argv || !argv[cursor] ||
           !isSwitch(argv[cursor]))return false;
        if(cursor+1>=argc || !argv[cursor+1]) {
            error="missing value for "+std::string(argv[cursor]);return false;
        }
        Options next=*this;
        if(!next.select(std::string(argv[cursor]+1),argv[cursor+1])) {
            error="duplicate or conflicting Lua driver selector";return false;
        }
        *this=std::move(next);++cursor;error.clear();return true;
    }
    static bool canonicalProgram(const std::string &path) {
        if(path.size()>255 || path.size()<4 || path[0]<'A' || path[0]>'Z' ||
           path[1]!=':' || path[2]!='/')return false;
        size_t begin=3;
        while(begin<path.size()) {
            const size_t slash=path.find('/',begin);
            const size_t end=slash==std::string::npos?path.size():slash;
            size_t base=0,extension=0;bool dot=false;
            for(size_t i=begin;i<end;++i) {
                const unsigned char c=path[i];
                if(c=='.') {if(dot || !base)return false;dot=true;continue;}
                // Explicit conservative DOS ASCII names. No shell metachar,
                // aliases, wildcard, lowercase or traversal normalization.
                if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_'))return false;
                if(dot)++extension;else ++base;
            }
            if(!base || base>8 || extension>3 || (dot && !extension))return false;
            if(slash==std::string::npos)return true;
            begin=slash+1;
        }
        return false;
    }
    bool valid(bool legacy_script,bool other_frontend) const {
        if(script==Script::None)return !program_set && !archive_set;
        if(legacy_script || other_frontend)return false;
        if(script==Script::File && source.empty())return false;
        if(archive_set && (!program_set || archive.empty()))return false;
        return !program_set || canonicalProgram(program);
    }
};
}
#endif

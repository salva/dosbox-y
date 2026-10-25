/* GPL-2.0-or-later. Dedicated selection value tests, not startup trials. */
#include "lua_driver_options.h"
#include <cassert>
using LuaDriver::Options;
int main() {
    Options o;assert(o.valid(false,false));
    assert(o.select("lua-driver-code",""));assert(o.valid(false,false));
    assert(!o.select("lua-driver-file","other"));
    assert(!o.valid(true,false));assert(!o.valid(false,true));
    assert(o.select("lua-driver-state","-archive"));assert(!o.valid(false,false));
    assert(o.select("lua-driver-program","C:/BIN/PROG.COM"));assert(o.valid(false,false));
    assert(!o.select("lua-driver-state","again"));
    assert(!o.select("lua-driver-program","C:/OTHER.COM"));
    Options p;assert(p.select("lua-driver-program","C:/PROG.COM"));assert(!p.valid(false,false));
    assert(Options::canonicalProgram("C:/A.COM"));
    for(const char *bad:{"c:/A.COM","C:\\A.COM","C:/../A.COM","C:/A.COM/", "C:/ABCDEFGHI.COM", "C:/A.ABCD","C:/A..COM","C:/a.COM","C:/A*.COM"})
        assert(!Options::canonicalProgram(bad));
    Options parsed;std::string error;int cursor=1;
    const char *argv[]={"dosbox","-lua-driver-code","-x","-lua-driver-program","C:/A.COM"};
    assert(parsed.consume(5,argv,cursor,error) && cursor==2 && parsed.source=="-x");
    ++cursor;assert(parsed.consume(5,argv,cursor,error) && cursor==4);
    assert(parsed.valid(false,false));
    cursor=1;assert(!parsed.consume(5,argv,cursor,error) && cursor==1 && !error.empty());
    const char *missing[]={"dosbox","-lua-driver-state"};cursor=1;
    assert(!parsed.consume(2,missing,cursor,error) && cursor==1 && !parsed.archive_set);
    const char *empty[]={"dosbox","-lua-driver-code",""};cursor=1;Options chunk;
    assert(chunk.consume(3,empty,cursor,error) && chunk.valid(false,false));
    Options dash;assert(dash.select("lua-driver-code","-x"));assert(dash.valid(false,false));
}

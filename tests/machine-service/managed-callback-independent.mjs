// Focused independent callback-denial variant; no production changes.
import fs from 'node:fs';
let source=fs.readFileSync('tests/machine-service/managed-session-saivage.mjs','utf8');
const start=source.indexOf(' const contextScript=');
const end=source.indexOf('\n fs.writeFileSync',start);
if(start<0||end<start)throw Error('unrecognized fixture');
const code=`local d=require('dosbox');local s=d.step();local r=d.registers(s.context);local calls=0;local denied=false;d.breakpoint(r.registers.cs,r.registers.ip+1,function(image) calls=calls+1;assert(d.read(image.context,r.registers.cs*16+r.registers.ip,1));local ok,err=pcall(d.step);denied=not ok and tostring(err):find('observer effect refused',1,true)~=nil;return true end);local hit=d.run();assert(calls==1 and denied and hit.reason=='error');return {calls=calls,denied=denied,reason=hit.reason,error=hit.error}`;
source=source.slice(0,start)+' const contextScript='+JSON.stringify(code)+';'+source.slice(end);
await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));

// Actual installed Saivage manager/provider/bound-tool qualification; no models.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {spawn, execFileSync} from 'node:child_process';
const release=process.env.SAIVAGE_RELEASE||'/opt/saivage';
const load=p=>import(pathToFileURL(path.join(release,'dist/src',p)));
const {McpManager}=await load('mcp/mcp-manager.js');
const {ProcessRunner}=await load('runtime/process-runner.js');
const {ManagedProcessGroupRegistry}=await load('runtime/managed-process-group-registry.js');
const {createResolvedConfigAuthority}=await load('config/index.js');
const {mcpToolBinders}=await load('tools/mcp-provider.js');
const {invokeToolForLlm}=await load('tools/invocation.js');
const {settleToolActionOutcome}=await load('tools/tool-result-settlement.js');
const w=fs.mkdtempSync(path.join(process.env.SAIVAGE_CARD_WORK_ROOT,'saivage-consumer-'));
console.log('work',w,'consumer',release,'pid',process.pid);
const binary=process.env.DOSBOX_TEST_BINARY,prefix=process.env.DOSBOX_SESSION_INSTALL;
fs.mkdirSync(path.join(w,'.saivage'),{recursive:true});
const configPath=path.join(w,'.saivage/saivage.yaml');
fs.writeFileSync(configPath,'mcpServers: {}\n');
fs.writeFileSync(path.join(w,'TINY.COM'),Buffer.from('b82a4ccd21','hex'));
fs.writeFileSync(path.join(w,'fixture.conf'),`[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n[autoexec]\nmount c ${w}\nc:\nTINY.COM\n`);
fs.writeFileSync(path.join(w,'probe.c'),'#include <lua.h>\nint luaopen_probe(lua_State *L){lua_pushinteger(L,42);return 1;}\n');
execFileSync('cc',['-shared','-fPIC',path.join(w,'probe.c'),'-o',path.join(w,'probe.so'),...execFileSync('pkg-config',['--cflags','--libs','lua5.4'],{encoding:'utf8'}).trim().split(/\s+/)]);
const xlog=fs.openSync(path.join(w,'xvfb.log'),'w');
const x=spawn('Xvfb',['-displayfd','3','-screen','0','1024x768x24'],{stdio:['ignore',xlog,xlog,'pipe']});
const display=await new Promise((resolve,reject)=>{x.stdio[3].once('data',b=>resolve(':'+b.toString().trim()));x.once('error',reject);});
const registry=new ManagedProcessGroupRegistry();
const runner=new ProcessRunner(w,registry,{publicationOutcomeUnknown(e){throw e;}});
const manager=new McpManager({configAuthority:createResolvedConfigAuthority({path:configPath,projectRoot:w,interpolationEnvironment:{}}),processRunner:runner,mcpProcessRootScope:registry.createContainerScope(registry.rootScope,'mcp'),eventLogger:{appendEventPrepared(){}}});
const tools=mcpToolBinders.map(b=>b.bind({projectRoot:w,mcpToolInvocation:manager}));
const surface={agentName:'executor',tools:new Map(tools.map(t=>[t.name,t])),providers:[{providerName:'mcp',tools}]};
async function bound(name,args,signal){
 const v=await invokeToolForLlm(surface,name,args,{sessionId:'agent:executor:project',sourceInputId:'isolated-consumer',toolName:name},signal);
 const r=settleToolActionOutcome(v.kind==='executed'?v.execution.providerOutcome:v.providerOutcome).providerResult;
 console.log(name,JSON.stringify(r));return r;
}
async function good(name,args){const r=await bound(name,args);assert.equal(r.success,true,JSON.stringify(r));return r.data;}
let catalog;
async function call(name,args,signal){
 const r=await bound('mcp_tool_call',{serverName:'lua',bindingId:catalog.bindingId,catalogId:catalog.catalogId,toolName:name,args},signal);
 assert.equal(r.success,!r.data?.result?.isError,JSON.stringify(r));return r.data.result.structuredContent;
}
const command=restore=>[binary,'-conf',path.join(w,'fixture.conf'),'-lua-driver-session',...(restore?['-restore-state',restore]:['-lua-driver-program','C:/TINY.COM'])];
async function create(cmd){const r=await call('session_create',cmd?{command:cmd}:{});assert.ok(r.session_id,JSON.stringify(r));return r.session_id;}
const execute=(sid,code,wait_timeout_ms=null)=>call('session_execute',{session_id:sid,code,wait_timeout_ms,execution_timeout_ms:20000});
async function kill(sid){const r=await call('session_kill',{session_id:sid});assert.equal(r.process_exit_confirmed,true);}
async function pidOf(sid){const r=await execute(sid,"local f=assert(io.open('/proc/self/stat'));local s=f:read('*a');f:close();return tonumber(s:match('^(%d+)'))");const pid=r.values[0];console.log('owned emulator',pid,'fds',fs.readdirSync(`/proc/${pid}/fd`).length);return pid;}
async function gone(pid){for(let i=0;i<100;i++){if(!fs.existsSync(`/proc/${pid}`)){console.log('owned process absent',pid);return;}await new Promise(r=>setTimeout(r,20));}throw Error(`owned process still present ${pid}`);}
try{
 await good('mcp_server_register',{serverName:'lua',connection:{transport:'stdio',command:path.join(prefix,'bin/lua-repl-mcp'),args:['--startup-timeout-ms','20000'],env:{DISPLAY:display,TERM:'xterm',SDL_AUDIODRIVER:'dummy',LUA_PATH:`${prefix}/share/lua/5.4/?.lua;${prefix}/share/lua/5.4/?/init.lua;;`,LUA_CPATH:`${w}/?.so;${prefix}/lib/lua/5.4/?.so;;`}}});
 await good('mcp_server_control',{serverName:'lua',action:'start'});
 catalog=await good('mcp_tools',{serverName:'lua'});assert.equal(catalog.tools.length,5);
 for(const cmd of [null,command()]){
  const sid=await create(cmd);
  assert.deepEqual((await execute(sid,"persistent=17;assert(require('probe')==42 and io and os and debug and coroutine);return require('lua_repl_mcp.bootstrap')~=nil,persistent")).values,[true,17]);
  assert.deepEqual((await execute(sid,'persistent=persistent+1;return persistent')).values,[18]);
  const once=path.join(w,cmd?'custom-once':'default-once');
  assert.equal((await execute(sid,`local f=assert(io.open(${JSON.stringify(once)},'a'));f:write('x');f:close();error('once-marker')`)).status,'failed');
  assert.equal(fs.readFileSync(once,'utf8'),'x');
  if(cmd){
   assert.deepEqual((await execute(sid,"local d=require('dosbox');c=d.context();return d.registers(c).registers_valid")).values,[true]);
   assert.deepEqual((await execute(sid,"return os.execute('test ! -e /proc/self/fd/3')")).values.slice(0,1),[true]);
   assert.deepEqual((await execute(sid,`return require('dosbox').save(${JSON.stringify(path.join(w,'source.sav'))}).effects.completed`)).values,[true]);
  }
  assert.equal((await execute(sid,'while true do end',0)).status,'running');
  assert.equal((await call('session_wait',{session_id:sid,wait_timeout_ms:1})).status,'running');await kill(sid);
 }
 const contextScript=`local d=require('dosbox'); old=d.context(); local s=d.step(); local r=d.registers(s.context); assert(not pcall(d.registers,old)); callback_count=0; bp=d.breakpoint(r.registers.cs,r.registers.ip+1,function(image) callback_count=callback_count+1; assert(d.read(image.context,r.registers.cs*16+r.registers.ip,1)); callback_denied=true; return true end); local hit=d.run(); assert(callback_count==1 and hit.reason=='breakpoint'); return {reason=hit.reason,calls=callback_count,denied=callback_denied,error=hit.error}`;
 fs.writeFileSync(path.join(w,'TINY.COM'),Buffer.from('9040ebfc','hex'));
 const debugSid=await create(command());
 const debugResult=await execute(debugSid,contextScript);assert.equal(debugResult.status,'completed');assert.equal(debugResult.values[0].calls,1);console.log('debugger context',JSON.stringify(debugResult));
 await kill(debugSid);
 const sid=await create(command(path.join(w,'source.sav')));
 const before=await execute(sid,"local d=require('dosbox');c=d.context();local r=d.registers(c);return {ip=r.registers.ip,cs=r.registers.cs}");
 for(const file of ['held1.sav','held2.sav'])assert.deepEqual((await execute(sid,`return require('dosbox').save(${JSON.stringify(path.join(w,file))}).effects.completed`)).values,[true]);
 execFileSync('python3',['-c',"import zipfile,sys; a,b=map(zipfile.ZipFile,sys.argv[1:]); assert all(a.read(n)==b.read(n) for n in ('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10'))",path.join(w,'held1.sav'),path.join(w,'held2.sav')]);
 // LP64 legacy MemoryBlock stores phandlers/mhandles at offsets 32/40.
 // Those process-local pointers relocate; compare all other bytes, including RAM.
 execFileSync('python3',['-c',"import zipfile,sys; a,b=map(zipfile.ZipFile,sys.argv[1:]); names=('CPU','Memory','Pic','Dosbox-x','Keyboard','IntTimer10'); norm=lambda n,x:x[:32]+bytes(16)+x[48:] if n=='Memory' else x; assert all(norm(n,a.read(n))==norm(n,b.read(n)) for n in names)",path.join(w,'source.sav'),path.join(w,'held1.sav')]);
 assert.deepEqual((await execute(sid,"local d=require('dosbox');local r=d.registers(d.context());return {ip=r.registers.ip,cs=r.registers.cs}")).values,before.values);
 const pending=await execute(sid,"local r=require('dosbox').run();return {reason=r.reason,exit_code=r.exit_code}",0);
 const terminal=pending.status==='running'?await call('session_wait',{session_id:sid,wait_timeout_ms:null}):pending;
 assert.equal(terminal.values[0].reason,'root_exit');assert.equal(terminal.values[0].exit_code,42);
 const late=await call('session_wait',{session_id:sid,wait_timeout_ms:0});assert.equal(late.error.code,'result_unavailable');console.log('late wait',JSON.stringify(late));
 await kill(sid);console.log('session list',JSON.stringify(await call('session_list',{})));
 for(const mode of ['deadline','sigkill','idle-manager-stop','active-manager-stop','observer-kill','save-manager-stop','cancel']){
  const owned=await create(command());const pid=await pidOf(owned);
  if(mode==='deadline'){
   const r=await call('session_execute',{session_id:owned,code:'while true do end',wait_timeout_ms:null,execution_timeout_ms:20});assert.equal(r.status,'failed');console.log('deadline outcome',JSON.stringify(r));assert.equal(r.process_exit_confirmed,true);
  }else if(mode==='sigkill'){
   process.kill(pid,'SIGKILL');const k=await call('session_kill',{session_id:owned});assert.ok(k.process_exit_confirmed||k.error?.code==='unknown_session');
  }else if(mode==='observer-kill'){
   assert.equal((await execute(owned,"local d=require('dosbox');local s=d.step();local r=d.registers(s.context);d.breakpoint(r.registers.cs,r.registers.ip+1,function() while true do end end);d.run()",0)).status,'running');await kill(owned);
  }else if(mode==='save-manager-stop'){
   const partial=path.join(w,'interrupted.sav');assert.equal((await execute(owned,`return require('dosbox').save(${JSON.stringify(partial)})`,0)).status,'running');
   await good('mcp_server_control',{serverName:'lua',action:'stop'});console.log('interrupted archive exists',fs.existsSync(partial),'validity/commit unknown; never restored');
  }else if(mode==='cancel'){
   const ctl=new AbortController();const pending=bound('mcp_tool_call',{serverName:'lua',bindingId:catalog.bindingId,catalogId:catalog.catalogId,toolName:'session_execute',args:{session_id:owned,code:'while true do end',wait_timeout_ms:null}},ctl.signal);setTimeout(()=>ctl.abort(new Error('qualification cancellation')),100);
   const cancelled=await pending;assert.equal(cancelled.success,false);assert.match(cancelled.error,/qualification cancellation/);console.log('cancel outcome',cancelled.error);
  }else{
   if(mode==='active-manager-stop')assert.equal((await execute(owned,'while true do end',0)).status,'running');
   await good('mcp_server_control',{serverName:'lua',action:'stop'});
  }
  await gone(pid);console.log('PASS lifecycle',mode);
  if(mode.endsWith('manager-stop')||mode==='cancel'){
   await good('mcp_server_control',{serverName:'lua',action:'start'});catalog=await good('mcp_tools',{serverName:'lua'});
  }
 }
 await good('mcp_server_control',{serverName:'lua',action:'stop'});
 console.log('PASS actual Saivage default/custom/archive/terminal',w);
}finally{
 await manager.cleanupForApplicationStop();console.log('manager cleanup confirmed');
 x.kill('SIGTERM');await new Promise(resolve=>x.once('exit',resolve));fs.closeSync(xlog);
}

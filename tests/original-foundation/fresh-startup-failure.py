"""Actual startup malformed-owner rejection: process disposal, unchanged input.
Uses valid-CRC repacking; not an unchanged-destination/rollback test.
"""
import hashlib,os,pathlib,subprocess,tempfile,zipfile,pty,fcntl,termios,struct,select,time
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='failure-',dir=root/'fresh-native-entry'))
source=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
original=hashlib.sha256(source.read_bytes()).hexdigest()
cases=[('Mouse',lambda b:b[:-1],False),
       ('Mouse',lambda b:b[:-16]+b'\xff'+b[-15:],False),
       ('Mouse',lambda b:b[:-17]+b'\xff'+b[-16:],False),
       ('Pic',lambda b:b[:1],False),('Keyboard',lambda b:b+b'\xff',False),
       ('Keyboard',lambda b:b,True),('Keyboard',lambda b:None,False)]
if os.environ.get('DOSBOX_TEST_BIOS_LAYOUT'):
 # Compare native allocation identity, never import an allocator graph.
 # Valid CRC mutations cover every extent/role field before RAM install.
 with zipfile.ZipFile(source) as z:layout=z.read('BIOS layout')
 assert len(layout)>=28 and len(layout)%4==0
 for offset in range(0,len(layout),4):
  def wrong_layout(data,offset=offset):
   data=bytearray(data);data[offset]^=1;return bytes(data)
  cases.append(('BIOS layout',wrong_layout,False))
 for length in (0,3,24,len(layout)-1):
  cases.append(('BIOS layout',lambda data,length=length:data[:length],False))
 cases.append(('BIOS layout',lambda data:data+b'\0',False))
if os.environ.get('DOSBOX_TEST_EMS_MONITOR'):
 # Physical private monitor above 1MiB; corrupt all six controlled bytes.
 import re
 config=w/'ems-monitor.conf'
 config.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=false\n[midi]\nmididevice=none\n[config]\ndos=low,umb\n')
 result=subprocess.run(['gdb','-q','-batch','-ex','break DEBUG_HoldRestoredStartup','-ex','run','-ex',"p 'ems.cpp'::vcpi.private_area",'-ex','quit','--args',os.environ['DOSBOX_TEST_BINARY'],'-conf',str(config),'-restore-state',str(source)],capture_output=True,text=True,timeout=45,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
 (w/'ems-monitor.log').write_text(result.stdout+result.stderr)
 values=list(map(int,re.findall(r'\$\d+ = (\d+)',result.stdout)))
 assert result.returncode==0 and len(values)==1 and values[0]>=0x100000,(result.stdout,result.stderr)
 _,descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 for n in range(6):
  def wrong_ems_monitor(data,n=n):
   data=bytearray(data);data[descriptor+values[0]+0x2e00+n]^=1;return bytes(data)
  cases.append(('Memory',wrong_ems_monitor,False))
if os.environ.get('DOSBOX_TEST_XMS_CONTROL'):
 # Registered handles are followed by HMA ownership/global A20/count.
 import re,struct
 result=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
  '-ex','p 24+sizeof(xms_handles)/sizeof(xms_handles[0])*(sizeof(xms_handles[0].size)+sizeof(xms_handles[0].mem)+sizeof(xms_handles[0].locked)+sizeof(xms_handles[0].free))'],capture_output=True,text=True,check=True)
 control_offset=int(re.findall(r'\$\d+ = (\d+)',result.stdout)[0])
 with zipfile.ZipFile(source) as z:assert len(z.read('XMS'))==control_offset+6
 for field in (0,1):
  def bad_xms_bool(data,field=field):
   data=bytearray(data);data[control_offset+field]=2;return bytes(data)
  cases.append(('XMS',bad_xms_bool,False))
 for count in (-1,2147483647):
  def bad_xms_count(data,count=count):
   data=bytearray(data);struct.pack_into('<i',data,control_offset+2,count);return bytes(data)
  cases.append(('XMS',bad_xms_count,False))
 for n in range(6):cases.append(('XMS',lambda data,n=n:data[:control_offset+n],False))
if os.environ.get('DOSBOX_TEST_HMA_CURSOR'):
 # Offset is computed from configured ABI fields used by the original owner,
 # not found by matching cursor bytes in RAM or guessing sizeof(DOS_Block).
 import re,struct
 # Explicit codec order below includes only fields actually emitted, not the
 # enclosing native structs. See configured expression in this probe.
 expressions=['sizeof(dos.verify)+sizeof(dos.breakcheck)+sizeof(dos.echo)+sizeof(dos.direct_output)+sizeof(dos.internal_output)+sizeof(uselfn)+sizeof(dos_copybuf)+sizeof(dos.firstMCB)+sizeof(dos.errorcode)+sizeof(dos.return_code)+sizeof(dos.return_mode)+sizeof(dos.current_drive)+sizeof(dos.loaded_codepage)+sizeof(dos.version.major)+sizeof(dos.version.minor)+sizeof(countryNo)+sizeof(lfn_filefind_handle)+sizeof(bootdrive)+sizeof(dos_kernel_disabled)+sizeof(uint16_t)']
 result=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],'-ex','p '+expressions[0]],capture_output=True,text=True,check=True)
 offset=int(re.findall(r'\$\d+ = (\d+)',result.stdout)[0])
 with zipfile.ZipFile(source) as z:data=z.read('Dos')
 assert 0x100000 <= struct.unpack_from('<I',data,offset)[0] <= 0x10fff0
 for cursor in (1,0xffffffff,0x10fff1):
  def bad_hma_cursor(data,cursor=cursor,offset=offset):
   data=bytearray(data);struct.pack_into('<I',data,offset,cursor);return bytes(data)
  cases.append(('Dos',bad_hma_cursor,False))
 for n in range(4):cases.append(('Dos',lambda data,n=n,offset=offset:data[:offset+n],False))
if os.environ.get('DOSBOX_TEST_DOS_CALLBACKS'):
 import re
 config=w/'dos-callbacks.conf'
 config.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound='+('false' if os.environ.get('DOSBOX_TEST_AUDIO') else 'true')+'\n[midi]\nmididevice=none\n')
 script=w/'dos-callbacks.gdb'
 if os.environ.get('DOSBOX_TEST_CLEAR_TF'):
  config.write_text(config.read_text().replace('[cpu]\n','[cpu]\nclear trap flag on unhandled int 1=true\n'))
 script.write_text('set pagination off\nset confirm off\nbreak DEBUG_HoldRestoredStartup\nrun\n'+''.join(
     f"p 'dos.cpp'::test->native_callback[{role}].address\np 'dos.cpp'::test->native_callback[{role}].body.size()\n" for role in range(9))+"p 'dos.cpp'::test->native_ihseg_address\np 'dos.cpp'::test->native_ihseg_body.size()\np 'dos.cpp'::test->native_hma_cpm_body.size()\nquit\n")
 result=subprocess.run(['gdb','-q','-batch','-x',str(script),'--args',os.environ['DOSBOX_TEST_BINARY'],
     '-conf',str(config),'-restore-state',str(source)],capture_output=True,text=True,timeout=45,
     env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'))
 (w/'dos-callbacks.log').write_text(result.stdout+result.stderr)
 values=list(map(int,re.findall(r'\$\d+ = (\d+)',result.stdout)))
 assert result.returncode==0 and len(values)==21 and values[-1] in (0,5),(result.stdout,result.stderr)
 hma_size=values.pop()
 _,memory_descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 # XMS also writes the DOS CP/M far-jump mirror in physical HMA.
 for n in range(hma_size):
  def wrong_hma_cpm(data,n=n):
   data=bytearray(data);data[memory_descriptor+0x1000c0+n]^=1;return bytes(data)
  cases.append(('Memory',wrong_hma_cpm,False))
 for address,size in zip(values[::2],values[1::2]):
  assert address and 0<size<128,(address,size)
  for n in range(size):
   def wrong_dos_callback(data,address=address,n=n):
    data=bytearray(data);data[memory_descriptor+address+n]^=1;return bytes(data)
   cases.append(('Memory',wrong_dos_callback,False))
if os.environ.get('DOSBOX_TEST_MOUSE_SCALARS'):
 # These first fields are fixed uint16/uint16/bool/bool, with no registered PODs.
 # Keep ZIP CRC valid and consume the malformed representation in the real owner.
 for offset in (4,5):
  def bad_mouse_bool(data,offset=offset):
   data=bytearray(data);data[offset]=2;return bytes(data)
  cases.append(('Mouse',bad_mouse_bool,False))
if os.environ.get('DOSBOX_TEST_MOUSE_FINITE'):
 # Consume valid-CRC malformed fields in the configured real owner. Derive
 # ABI offsets/widths from that binary, not the reduced fixture's guessed POD.
 import re
 fields=('add_x','add_y','mickey_accum_x','mickey_accum_y','x','y','ps2x','ps2y',
         'mickeysPerPixel_x','mickeysPerPixel_y','pixelPerMickey_x','pixelPerMickey_y','senv_x','senv_y')
 expressions=["sizeof('mouse.cpp'::ps2cbseg)*2+sizeof('mouse.cpp'::useps2callback)*2+sizeof('mouse.cpp'::aux_pkt)+sizeof('mouse.cpp'::aux_pkt_len)+sizeof('mouse.cpp'::userdefScreenMask)+sizeof('mouse.cpp'::userdefCursorMask)"]
 expressions += [f"(char*)&'mouse.cpp'::mouse.{field}-(char*)&'mouse.cpp'::mouse" for field in (*fields,'hidden_at')]
 expressions += ["sizeof('mouse.cpp'::mouse.hidden_at)"]
 result=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
     *[arg for expression in expressions for arg in ('-ex','p '+expression)]],
     capture_output=True,text=True,check=True)
 values=list(map(int,re.findall(r'\$\d+ = (\d+)',result.stdout)))
 assert len(values)==len(fields)+3,result.stdout
 prefix,*offsets,hidden_offset,hidden_width=values
 for offset in offsets:
  for value in (float('inf'),float('-inf'),float('nan')):
   def bad_mouse_finite(data,offset=offset,value=value):
    data=bytearray(data);struct.pack_into('<f',data,prefix+offset,value);return bytes(data)
   cases.append(('Mouse',bad_mouse_finite,False))
 # Native Linux x86 receipts use x87 extended precision in a 16-byte slot;
 # also support pic.h's IEEE-double ABI, rejecting unrepresented layouts.
 import platform,sys
 assert hidden_width in (8,16) and sys.byteorder=='little'
 assert hidden_width==8 or platform.machine() in ('x86_64','i386','i686')
 hidden_values=([struct.pack('<d',value) for value in (float('inf'),float('-inf'),float('nan'))]
                if hidden_width==8 else
                [bytes.fromhex(value)+bytes(6) for value in
                 ('0000000000000080ff7f','0000000000000080ffff','00000000000000c0ff7f')])
 for encoded in hidden_values:
  def bad_mouse_tick(data,encoded=encoded):
   data=bytearray(data);data[prefix+hidden_offset:prefix+hidden_offset+hidden_width]=encoded;return bytes(data)
  cases.append(('Mouse',bad_mouse_tick,False))
if os.environ.get('DOSBOX_TEST_MOUSE_BINDINGS'):
 import re
 commands=[]
 for role in range(6):
  commands += ['-ex',f"p 'mouse.cpp'::mouse_native_roles[{role}].address",
               '-ex',f"p 'mouse.cpp'::mouse_native_roles[{role}].body.size()"]
 # Constructor values require running to held restored startup first.
 script=w/'mouse-bindings.gdb'
 script.write_text('set pagination off\nset confirm off\nset breakpoint pending on\nbreak DEBUG_HoldRestoredStartup\nrun\n'+
     '\n'.join(commands[n+1] for n in range(0,len(commands),2))+'\nquit\n')
 # Use the same initialized matching target arguments as the failure runner.
 binding_config=w/'mouse-bindings.conf'
 binding_config.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound='+('false' if os.environ.get('DOSBOX_TEST_AUDIO') else 'true')+'\n[midi]\nmididevice=none\n')
 # Native role addresses are also visible in an ordinary initialized paused
 # producer; using the archive startup avoids any bootstrap guest execution.
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 result=subprocess.run(['gdb','-q','-batch','-x',str(script),'--args',os.environ['DOSBOX_TEST_BINARY'],
     '-conf',str(binding_config),'-restore-state',str(source)],env=env,capture_output=True,text=True,timeout=45)
 (w/'mouse-bindings.log').write_text(result.stdout+result.stderr)
 values=list(map(int,re.findall(r'\$\d+ = (\d+)',result.stdout)))
 assert result.returncode==0 and len(values)==12,(result.stdout,result.stderr)
 _,mouse_descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 for address,size in zip(values[::2],values[1::2]):
  assert address and 0<size<128,(address,size)
  for n in range(size):
   def wrong_mouse_body(data,address=address,n=n):
    data=bytearray(data);data[mouse_descriptor+address+n]^=1;return bytes(data)
   cases.append(('Memory',wrong_mouse_body,False))
if os.environ.get('DOSBOX_TEST_DOS_TABLES'):
 # DOS tables are the final Dos suffix. Observe actual configured owner
 # values at held import; verify the entire fixed binding prefix occurs once.
 import re
 expressions=('dos.tables.mediaid','dos.tables.tempdta','dos.tables.tempdta_fcbdelete',
  'dos.tables.dbcs','dos.tables.filenamechar','dos.tables.collatingseq',
  'dos.tables.upcase','dos.tables.dpb','dos.tables.dpb_size','dos.tables.mediaid_offset',
  'DOS_TableUpCase','DOS_TableLowCase','DOS_PRIVATE_SEGMENT','DOS_PRIVATE_SEGMENT_END',
  'DOS_DriveDataListHead',"'dos_tables.cpp'::call_casemap",'DOS_SDA_SEG','DOS_SDA_OFS',
  'DOS_SDA_SEG_SIZE','DOS_INFOBLOCK_SEG','DOS_CONDRV_SEG','DOS_CDS_SEG','DOS_IHSEG','cmd_line_seg')
 script=w/'dos-tables.gdb'
 script.write_text('set pagination off\nset confirm off\nset breakpoint pending on\nbreak DEBUG_HoldRestoredStartup\nrun\n'+
  '\n'.join('p (unsigned int)'+expr for expr in expressions)+'\nquit\n')
 config=w/'dos-tables.conf'
 config.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound='+('false' if os.environ.get('DOSBOX_TEST_AUDIO') else 'true')+'\n[midi]\nmididevice=none\n')
 result=subprocess.run(['gdb','-q','-batch','-x',str(script),'--args',os.environ['DOSBOX_TEST_BINARY'],
  '-conf',str(config),'-restore-state',str(source)],capture_output=True,text=True,
  env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),timeout=45)
 (w/'dos-tables.log').write_text(result.stdout+result.stderr)
 values=list(map(int,re.findall(r'\$\d+ = (\d+)',result.stdout)))
 assert result.returncode==0 and len(values)==24,(result.stdout,result.stderr)
 with zipfile.ZipFile(source) as z:data=z.read('Dos')
 needle=struct.pack('<24I',*values);offset=data.find(needle)
 assert offset>=0 and data.find(needle,offset+1)<0
 # The no-CD MSCDEX inventory and native buffer/header segments immediately
 # precede the table suffix. Prove the absent-service wire shape before using
 # its boundary; mutate each field and truncate every consumed prefix.
 mscdex_offset=offset-6
 assert mscdex_offset>=0 and data[mscdex_offset:offset]==bytes(6)
 for n in range(6):
  def wrong_mscdex(data,n=n,start=mscdex_offset):
   data=bytearray(data);data[start+n]^=1;return bytes(data)
  cases.append(('Dos',wrong_mscdex,False))
 for n in range(mscdex_offset,offset):
  cases.append(('Dos',lambda data,n=n:data[:n],False))
 for n in range(len(needle)):
  def wrong_dos_table(data,n=n,table_offset=offset):
   data=bytearray(data);data[table_offset+n]^=1;return bytes(data)
  cases.append(('Dos',wrong_dos_table,False))
 # All short suffixes, including allocator role records/country/cursor, must
 # fail after earlier DOS/file/memory owners have already been consumed.
 for n in range(offset,len(data)):
  cases.append(('Dos',lambda data,n=n:data[:n],False))
if os.environ.get('DOSBOX_TEST_MIXER_TRUNCATION'):
 # Progressive native audio-owner chain has already installed earlier owners.
 # Repack with valid CRC; truncations across the complete Mixer payload must
 # dispose the disposable target rather than fall through to worker/guest.
 with zipfile.ZipFile(source) as z:mixer=z.read('Mixer')
 cuts=sorted(set((0,1,len(mixer)//4,len(mixer)//2,3*len(mixer)//4,len(mixer)-1)))
 for length in cuts:
  cases.append(('Mixer',lambda data,length=length:data[:length],False))
if os.environ.get('DOSBOX_TEST_CORE_TRUNCATION'):
 # Real transport/CRC, short consumed owner at two different dependency cuts.
 # No unchanged-destination assumption: every attempt is a fresh process.
 for name in ('CPU','Memory'):
  cases.append((name,lambda b:b[:1],False))
if os.environ.get('DOSBOX_TEST_CPU_BOOLS'):
 # Derive serialized extents from this configured ABI; C++ object padding
 # between CPUBlock members is not part of the field-by-field owner codec.
 import re
 exprs=['sizeof(cpu_regs)+sizeof(cpu.cpl)+sizeof(cpu.mpl)+sizeof(cpu.cr0)',
        'sizeof(LinearPt)+sizeof(Bitu)',
        'sizeof(LinearPt)+sizeof(Bitu)*2',
        'sizeof(cpu.stack)', 'sizeof(cpu.code)', 'sizeof(cpu.hlt.cs)*2',
        'sizeof(cpu.exception)', 'sizeof(cpu.direction)',
        '(char*)&cpu.stack.big-(char*)&cpu.stack']
 result=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
                        *[arg for expr in exprs for arg in ('-ex','p '+expr)]],
                       capture_output=True,text=True,check=True)
 pmode,table,ldt,stack,code,hlt,exception,direction,big=map(int,re.findall(r'\$\d+ = (\d+)',result.stdout))
 stack_start=pmode+1+table*2+ldt
 offsets=(pmode,stack_start+big,stack_start+stack,
          stack_start+stack+code+hlt+exception+direction)
 extra_exprs=['sizeof(cpu.drx)+sizeof(cpu.trx)+sizeof(Segs)+sizeof(CPU_Cycles)+sizeof(CPU_CycleLeft)+sizeof(CPU_IODelayRemoved)',
              'sizeof(cpu_tss.desc.saved)+sizeof(cpu_tss.selector)+sizeof(cpu_tss.base)+sizeof(cpu_tss.limit)',
              'sizeof(cpu_tss.is386)']
 extra=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
                       *[arg for expr in extra_exprs for arg in ('-ex','p '+expr)]],
                      capture_output=True,text=True,check=True)
 after_trap,tss_prefix,is386_width=map(int,re.findall(r'\$\d+ = (\d+)',extra.stdout))
 tss_is386=offsets[-1]+1+after_trap+tss_prefix
 for offset in (*offsets,tss_is386+is386_width):
  def bad_cpu_bool(data,offset=offset):
   data=bytearray(data);assert data[offset] in (0,1)
   data[offset]=2;return bytes(data)
  cases.append(('CPU',bad_cpu_bool,False))
 def bad_tss_kind(data):
  data=bytearray(data);data[tss_is386:tss_is386+is386_width]=(2).to_bytes(is386_width,'little');return bytes(data)
 cases.append(('CPU',bad_tss_kind,False))
if os.environ.get('DOSBOX_TEST_DBOPL'):
 # Extract actual configured ABI extent, not a guessed packed Chip layout.
 import re
 result=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
                        '-ex','p sizeof(DBOPL::Chip)','-ex','p sizeof(Bitu)'],
                       capture_output=True,text=True,check=True)
 chip_size,bitu_size=map(int,re.findall(r'\$\d+ = (\d+)',result.stdout))
 scalar_fields=('vibratoIndex','tremoloIndex','vibratoStrength','tremoloStrength','lfoAdd','lfoCounter')
 offsets=subprocess.run(['gdb','-q','-batch',os.environ['DOSBOX_TEST_BINARY'],
     *[arg for field in scalar_fields for arg in ('-ex',f'p (unsigned long)&((DBOPL::Chip*)0)->{field}')]],
     capture_output=True,text=True,check=True)
 scalar_offsets=dict(zip(scalar_fields,map(int,re.findall(r'\$\d+ = (\d+)',offsets.stdout))))
 assert len(scalar_offsets)==len(scalar_fields),offsets.stdout
 def bad_opl_scalar(data,field):
  data=bytearray(data);start=data.index(b'DBOPL'+bytes(27))+32+8192+1
  width=4 if field in ('lfoAdd','lfoCounter') else 1
  value=0 if field=='lfoAdd' else 255
  offset=start+scalar_offsets[field]
  data[offset:offset+width]=bytes([value])*width
  return bytes(data)
 for field in scalar_fields:
  cases.append(('Mixer',lambda data,field=field:bad_opl_scalar(data,field),False))
 def bad_opl_role(data,kind):
  data=bytearray(data);start=data.index(b'DBOPL'+bytes(27))
  roles=start+32+8192+1+chip_size
  offset=roles if kind=='volume' else roles+36 if kind=='wave' else roles+36+36*bitu_size
  width=bitu_size if kind=='wave' else 1
  data[offset:offset+width]=bytes([255])*width
  return bytes(data)
 for kind in ('volume','wave','synth'):
  cases.append(('Mixer',lambda data,kind=kind:bad_opl_role(data,kind),False))
if os.environ.get('DOSBOX_TEST_SEARCH_STATE'):
 def search_field(data, field):
  # Search path is the mounted producer directory plus DOS separator. Locate
  # the actual represented active slot, not an ABI-sized guessed DOS prefix.
  path=str(source.parent).upper().encode()+b'/'
  candidates=[i for i in range(len(data)) if data.startswith(path,i) and i and data[i-1]==1]
  # Several simultaneous DTA slots may name this same directory. Damage
  # the first represented slot; the owning consumer must reject immediately.
  assert candidates,candidates
  start=candidates[0]; data=bytearray(data)
  if field=='tag':data[start-1]=2
  elif field=='outside':data[start:start+len(path)]=b'Z'*len(path)
  elif field=='truncated':return bytes(data[:start+1])
  else:raise AssertionError(field)
  return bytes(data)
 for field in ('tag','outside','truncated'):
  cases.append(('Dos',lambda data,field=field:search_field(data,field),False))
if os.environ.get('DOSBOX_TEST_FILE_STATE'):
 def missing_native_file(data):
  assert b'FLAG.TXT' in data
  return data.replace(b'FLAG.TXT',b'GONE.TXT')
 assert not (source.parent/'GONE.TXT').exists()
 cases.append(('Dos',missing_native_file,False))
if os.environ.get('DOSBOX_TEST_BINDINGS'):
 def wrong_binding(data):
  assert data
  return bytes([data[0]^1])+data[1:]
 for name in ('BIOS layout','Shell bindings','XMS','EMS'):
  cases.append((name,wrong_binding,False))
 if os.environ.get('DOSBOX_TEST_XMS_EMS_BINDING_FIELDS'):
  # Prefixes belong to the original XMS/EMS codecs, not a second decoder.
  # Every role/location/span field is consumed before mutable service state.
  for owner,count in (('XMS',6),('EMS',9)):
   for offset in range(0,count*4,4):
    def wrong_service_field(data,offset=offset):
     data=bytearray(data);data[offset]^=1;return bytes(data)
    cases.append((owner,wrong_service_field,False))
   for length in range(count*4):
    cases.append((owner,lambda data,length=length:data[:length],False))
 if os.environ.get('DOSBOX_TEST_SHELL_BINDING_FIELDS'):
  # Each role has independent slot/type/location/span identity. Mutate every
  # uint32 field with valid ZIP CRC; a count-only check would miss most cases.
  with zipfile.ZipFile(source) as z:
   shell_binding=z.read('Shell bindings')
  assert len(shell_binding)==13*4
  for offset in range(0,len(shell_binding),4):
   def wrong_shell_field(data,offset=offset):
    data=bytearray(data);data[offset]^=1;return bytes(data)
   cases.append(('Shell bindings',wrong_shell_field,False))
  for length in range(len(shell_binding)):
   cases.append(('Shell bindings',lambda data,length=length:data[:length],False))
 # The metadata still matches. Corrupt one owner-controlled trampoline byte
 # inside the saved RAM; Shell bindings must catch this after Memory installs.
 with zipfile.ZipFile(source) as z:
  fields=struct.unpack('<13I',z.read('Shell bindings'))
 import re
 _,descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 def wrong_native_body(data):
  offset=descriptor+fields[3]
  assert 0<=offset<len(data)
  data=bytearray(data);data[offset]^=1
  return bytes(data)
 cases.append(('Memory',wrong_native_body,False))
 if os.environ.get('DOSBOX_TEST_SHELL_BINDING_FIELDS'):
  # Certify each represented controlled span after RAM installation. Do not
  # mutate PSP vectors/guest hooks: those are intentionally not native bodies.
  for role in (1,5,9):
   address,span=fields[role+2:role+4]
   assert span in (5,6)
   for byte in range(span):
    def wrong_shell_body(data,address=address,byte=byte):
     offset=descriptor+address+byte
     assert 0 <= offset < len(data)
     data=bytearray(data);data[offset]^=1;return bytes(data)
    cases.append(('Memory',wrong_shell_body,False))
 # Matching metadata must not hide a damaged native service body in RAM.
 # XMS permits guest edits in its five-byte hook prefix, not its dispatch body.
 with zipfile.ZipFile(source) as z:
  xms_fields=struct.unpack('<6I',z.read('XMS')[:24])
  ems_fields=struct.unpack('<9I',z.read('EMS')[:36])
 assert xms_fields[0] and xms_fields[2] and ems_fields[7]
 def wrong_xms_body(data):
  real=xms_fields[2]
  offset=descriptor+((real>>16)*16+(real&0xffff))+5
  assert 0<=offset<len(data)
  data=bytearray(data);data[offset]^=1
  return bytes(data)
 def wrong_ems_body(data):
  offset=descriptor+ems_fields[7]
  assert 0<=offset<len(data)
  data=bytearray(data);data[offset]^=1
  return bytes(data)
 cases.extend(('Memory',mutation,False) for mutation in (wrong_xms_body,wrong_ems_body))
if os.environ.get('DOSBOX_TEST_INT10_BODY'):
 with zipfile.ZipFile(source) as z:
  shell=struct.unpack('<13I',z.read('Shell bindings'))
  slot=struct.unpack_from('<I',z.read('Int10'))[0]
 import re
 _,int10_descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 # Same producer callback arena, encoded slot locations; no host addresses.
 address=shell[3]+(slot-shell[1])*32
 def wrong_int10_body(data,address=address):
  data=bytearray(data);offset=int10_descriptor+address
  assert data[offset:offset+2]==b'\xfe\x38'
  data[offset]^=1;return bytes(data)
 cases.append(('Memory',wrong_int10_body,False))
if os.environ.get('DOSBOX_TEST_VESA_BODY'):
 import re,struct
 with zipfile.ZipFile(source) as z:
  fields=struct.unpack('<30I',z.read('Int10')[:120])
 _,vesa_descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 start=(fields[17]>>16)*16+(fields[17]&0xffff)
 end=(fields[19]>>16)*16+(fields[19]&0xffff)+fields[20]
 assert 0xc0000 <= start < end < 0xd0000,(start,end)
 # Wait code, native real dispatch and PM interface header/tail are all
 # constructor-controlled; valid CRC cannot authorize a changed binding body.
 for address in (start,start+23,(fields[18]>>16)*16+(fields[18]&0xffff),end-1):
  def wrong_vesa_body(data,address=address):
   data=bytearray(data);data[vesa_descriptor+address]^=1;return bytes(data)
  cases.append(('Memory',wrong_vesa_body,False))
if os.environ.get('DOSBOX_TEST_ROOT_VALIDATION'):
 # The producer fixture exposes its actual root explicitly; corrupt installed
 # guest PSP/MCB bytes, not the binding metadata. DOS must reject after Memory.
 psp=int(os.environ['DOSBOX_TEST_ROOT_PSP'],16)
 import re
 _,root_descriptor=map(int,re.findall(r'\$\d+ = (\d+)',(source.parent/'layout.txt').read_text()))
 def bad_psp_signature(data):
  data=bytearray(data);offset=root_descriptor+psp*16
  assert data[offset:offset+2]==b'\xcd\x20'
  data[offset]=0
  return bytes(data)
 def bad_root_mcb_owner(data):
  data=bytearray(data);offset=root_descriptor+(psp-1)*16+1
  assert int.from_bytes(data[offset:offset+2],'little')==psp
  data[offset:offset+2]=b'\x00\x00'
  return bytes(data)
 def bad_psp_parent(data):
  data=bytearray(data);offset=root_descriptor+psp*16+0x16
  data[offset:offset+2]=b'\x00\x00'
  return bytes(data)
 def bad_psp_stack(data):
  data=bytearray(data);offset=root_descriptor+psp*16+0x2e
  data[offset:offset+4]=b'\xff\xff\xff\xff'
  return bytes(data)
 def bad_psp_vector(data):
  data=bytearray(data);offset=root_descriptor+psp*16+0x0a
  data[offset:offset+4]=b'\xff\xff\xff\xff'
  return bytes(data)
 root_mutations=(bad_psp_signature,bad_root_mcb_owner,bad_psp_parent,bad_psp_stack,bad_psp_vector)
 cases.extend(('Memory',mutation,False) for mutation in root_mutations)
metadata={'Archive_Revision','Build_Identity','Config_Identity'}
if os.environ.get('DOSBOX_TEST_METADATA'):
 for name in sorted(metadata):
  cases.append((name,lambda b:b[:-1],False))
if os.environ.get('DOSBOX_TEST_CHANGED_BASIC_ROM'):
 # Archive unchanged: the target loads different immutable bytes at a different
 # path. This must fail identity before any owner install or evaluator.
 cases=[('Config_Identity',lambda b:b,False)]
for case_index,(owner,mutate,corrupt_crc) in enumerate(cases):
 missing=owner=='Keyboard' and mutate(b'') is None
 label=str(case_index)+'-'+owner+('-CRC' if corrupt_crc else '-missing' if missing else '')
 archive=w/(label+'.sav')
 with zipfile.ZipFile(source) as src,zipfile.ZipFile(archive,'w',zipfile.ZIP_STORED if corrupt_crc else zipfile.ZIP_DEFLATED) as dst:
  for name in src.namelist():
   data=src.read(name)
   payload=mutate(data) if name==owner else data
   if payload is not None:dst.writestr(name,payload)
 if corrupt_crc:
  # Keep an otherwise consumable owner but invalidate transport settlement.
  # Keyboard's last field is data, not a count or native binding.
  with zipfile.ZipFile(archive) as z:entry=z.getinfo(owner)
  data=bytearray(archive.read_bytes())
  name_size,extra_size=struct.unpack_from('<HH',data,entry.header_offset+26)
  offset=entry.header_offset+30+name_size+extra_size
  assert entry.file_size>0
  data[offset+entry.file_size-1]^=1
  archive.write_bytes(data)
 before=hashlib.sha256(archive.read_bytes()).hexdigest()
 conf=w/'fixture.conf'
 conf.write_text('[sdl]\noutput=surface\n[cpu]\ncore=normal\ncycles=fixed 3000\n[mixer]\nnosound=true\n[midi]\nmididevice=none\n')
 if os.environ.get('DOSBOX_TEST_CLEAR_TF'):
  conf.write_text(conf.read_text().replace('[cpu]\n','[cpu]\nclear trap flag on unhandled int 1=true\n'))
 if os.environ.get('DOSBOX_TEST_MEMSIZE'):
  memory=int(os.environ['DOSBOX_TEST_MEMSIZE'])
  assert 1 <= memory <= 256
  conf.write_text('[dosbox]\nmemsize='+str(memory)+'\n'+conf.read_text())
 if os.environ.get('DOSBOX_TEST_CYCLES'):
  cycles=int(os.environ['DOSBOX_TEST_CYCLES'])
  assert 1 <= cycles <= 1000000
  conf.write_text(conf.read_text().replace('cycles=fixed 3000',f'cycles=fixed {cycles}'))
 if os.environ.get('DOSBOX_TEST_CHANGED_BASIC_ROM'):
  rom=w/'different.rom';rom.write_bytes(bytes([0x91])*32768)
  conf.write_text(conf.read_text()+f'[dosbox]\nibm rom basic={rom}\n')
 if os.environ.get('DOSBOX_TEST_AUDIO'):
  conf.write_text(conf.read_text().replace('nosound=true','nosound=false'))
 if os.environ.get('DOSBOX_TEST_EMS_MONITOR'):
  conf.write_text(conf.read_text()+'[config]\ndos=low,umb\n')
 master,slave=pty.openpty()
 fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',50,160,0,0))
 os.set_blocking(master,False)
 marker=w/(label+'.evaluator-entered')
 args=[os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive)]
 if os.environ.get('DOSBOX_TEST_LUA'):
  script=f"local f=assert(io.open('{marker}','w')); f:write('entered'); f:close()"
  mode=os.environ.get('DOSBOX_TEST_LUA_MODE','code')
  assert mode in ('code','file')
  if mode=='file':
   evaluator=w/(label+'.lua');evaluator.write_text(script)
   script=str(evaluator)
  args+=['-lua-driver-'+mode,script]
 with (w/(label+'.log')).open('wb') as log:
  child=subprocess.Popen(args,stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'))
  os.close(slave)
  deadline=time.monotonic()+30
  while child.poll() is None:
   if time.monotonic()>deadline:
    child.kill();child.wait();raise RuntimeError('startup rejection timeout')
   if select.select([master],[],[],.05)[0]:
    try: log.write(os.read(master,65536))
    except OSError: pass
  while True:
   try:
    data=os.read(master,65536)
    if not data:break
    log.write(data)
   except OSError:break
  os.close(master)
 text=(w/(label+'.log')).read_text(errors='replace')
 assert child.returncode==1,(owner,child.returncode,text[-3000:])
 # This frontend can redirect stderr after SDL initialization. Diagnose the
 # exact native fatal boundary separately rather than accepting exit1 alone.
 diagnosis=subprocess.run(['gdb','-batch','-ex','break E_Exit','-ex','run','-ex','x/s $rsi','-ex','bt','--args',os.environ['DOSBOX_TEST_BINARY'],'-conf',str(conf),'-restore-state',str(archive)],capture_output=True,text=True,timeout=30,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TERM='xterm'))
 (w/(label+'.gdb.log')).write_text(diagnosis.stdout+diagnosis.stderr)
 assert diagnosis.returncode==0
 # Metadata refusal returns from install, then startup owns the fatal.
 assert 'SaveState::restoreFreshStartup' in diagnosis.stdout,diagnosis.stdout
 if owner in metadata:
  assert 'Fresh restore failed; disposable target cannot continue' in diagnosis.stdout,diagnosis.stdout
 else:
  assert 'SaveState::install' in diagnosis.stdout,diagnosis.stdout
 expected=('Fresh restore failed' if owner in metadata else
           'failed during installation' if mutate.__name__=='wrong_ems_monitor' else
           'failed during installation' if owner=='Dos' and mutate.__name__=='bad_hma_cursor' else
           'failed during installation' if owner=='Dos' and os.environ.get('DOSBOX_TEST_HMA_CURSOR') else
           'failed during installation' if owner=='Dos' and os.environ.get('DOSBOX_TEST_DOS_TABLES') else
           'Invalid save-state DOS drive state' if owner=='Dos' and os.environ.get('DOSBOX_TEST_SEARCH_STATE') else
           'Cannot restore save-state DOS file: native open failed' if owner=='Dos' else
           'State member unavailable after installation started' if missing else
           'settlement failed after installation' if corrupt_crc else
           'Invalid saved PIC state' if owner=='Pic' else
           'Invalid saved CPU state' if owner=='CPU' else
           'failed during installation' if owner=='Dos' else
           'failed during installation' if owner=='Memory' and mutate.__name__ in ('wrong_dos_callback','wrong_hma_cpm') else
           'failed during installation' if owner=='Memory' and mutate.__name__=='wrong_mouse_body' else
           'failed during installation' if owner=='Memory' and mutate.__name__=='wrong_shell_body' else
           'failed during installation' if owner=='Memory' and os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') and mutate in root_mutations else
           'failed during installation' if owner=='Memory' and os.environ.get('DOSBOX_TEST_INT10_BODY') and mutate is wrong_int10_body else
           'failed during installation' if owner=='Memory' and os.environ.get('DOSBOX_TEST_VESA_BODY') and mutate.__name__=='wrong_vesa_body' else
           'Invalid saved memory state' if owner=='Memory' and not (os.environ.get('DOSBOX_TEST_BINDINGS') and mutate in (wrong_native_body,wrong_xms_body,wrong_ems_body)) else
           'failed during installation')
 assert expected in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_BINDINGS'):
  for mutation,name in ((wrong_native_body,'Shell bindings'),(wrong_xms_body,'XMS'),(wrong_ems_body,'EMS')):
   if mutate is mutation:assert name in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') and mutate in root_mutations:
  assert 'Dos' in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_INT10_BODY') and mutate is wrong_int10_body:
  assert 'Int10' in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_VESA_BODY') and mutate.__name__=='wrong_vesa_body':
  assert 'Int10' in diagnosis.stdout,diagnosis.stdout
 assert not marker.exists(),(label,'evaluator ran after failed restore')
 assert 'Restored session ended:' not in text
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
assert hashlib.sha256(source.read_bytes()).hexdigest()==original
print(w,('changed loaded immutable ROM bytes' if os.environ.get('DOSBOX_TEST_CHANGED_BASIC_ROM') else 'malformed Mouse masks/truncated suffix, Pic/late Keyboard, late CRC settlement and missing late owner')+
      ('/truncated CPU and Memory' if os.environ.get('DOSBOX_TEST_CORE_TRUNCATION') else '')+
      ('/CPU pmode/stack/code/trap bool representations' if os.environ.get('DOSBOX_TEST_CPU_BOOLS') else '')+
      ('/DBOPL volume-wave-synth roles/LFO scalar bounds' if os.environ.get('DOSBOX_TEST_DBOPL') else '')+
      ('/active native search tag/path/truncation' if os.environ.get('DOSBOX_TEST_SEARCH_STATE') else '')+
      ('/native DOS file reopen' if os.environ.get('DOSBOX_TEST_FILE_STATE') else '')+
      ('/BIOS-shell-XMS-EMS bindings/native shell-XMS-EMS bodies' if os.environ.get('DOSBOX_TEST_BINDINGS') else '')+
      ('/VBE wait/real/PM constructor body' if os.environ.get('DOSBOX_TEST_VESA_BODY') else '')+
      ('/Int10 native trampoline body' if os.environ.get('DOSBOX_TEST_INT10_BODY') else '')+
      ('/PSP signature/MCB ownership/parent/stack/vector' if os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') else '')+
      ('/every BIOS allocator extent-role field and truncated/trailing layout' if os.environ.get('DOSBOX_TEST_BIOS_LAYOUT') else '')+
      ('/representation-build-config metadata' if os.environ.get('DOSBOX_TEST_METADATA') else '')+
      ' reject/exit1; source and attempt archives unchanged')

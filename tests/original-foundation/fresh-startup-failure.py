"""Actual startup malformed-owner rejection: process disposal, unchanged input.
Uses valid-CRC repacking; not an unchanged-destination/rollback test.
"""
import hashlib,os,pathlib,subprocess,tempfile,zipfile,pty,fcntl,termios,struct,select,time
root=pathlib.Path(os.environ['SAIVAGE_CARD_WORK_ROOT'])
w=pathlib.Path(tempfile.mkdtemp(prefix='failure-',dir=root/'fresh-native-entry'))
source=pathlib.Path(os.environ['DOSBOX_TEST_ARCHIVE'])
original=hashlib.sha256(source.read_bytes()).hexdigest()
cases=[('Pic',lambda b:b[:1],False),('Keyboard',lambda b:b+b'\xff',False),
       ('Keyboard',lambda b:b,True),('Keyboard',lambda b:None,False)]
if os.environ.get('DOSBOX_TEST_CORE_TRUNCATION'):
 # Real transport/CRC, short consumed owner at two different dependency cuts.
 # No unchanged-destination assumption: every attempt is a fresh process.
 for name in ('CPU','Memory'):
  cases.append((name,lambda b:b[:1],False))
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
 with zipfile.ZipFile(source) as src,zipfile.ZipFile(archive,'w',zipfile.ZIP_STORED) as dst:
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
           'Invalid save-state DOS drive state' if owner=='Dos' and os.environ.get('DOSBOX_TEST_SEARCH_STATE') else
           'Cannot restore save-state DOS file: native open failed' if owner=='Dos' else
           'State member unavailable after installation started' if missing else
           'settlement failed after installation' if corrupt_crc else
           'Invalid saved PIC state' if owner=='Pic' else
           'Invalid saved CPU state' if owner=='CPU' else
           'failed during installation' if owner=='Memory' and os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') and mutate in root_mutations else
           'Invalid saved memory state' if owner=='Memory' and not (os.environ.get('DOSBOX_TEST_BINDINGS') and mutate in (wrong_native_body,wrong_xms_body,wrong_ems_body)) else
           'failed during installation')
 assert expected in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_BINDINGS'):
  for mutation,name in ((wrong_native_body,'Shell bindings'),(wrong_xms_body,'XMS'),(wrong_ems_body,'EMS')):
   if mutate is mutation:assert name in diagnosis.stdout,diagnosis.stdout
 if owner=='Memory' and os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') and mutate in root_mutations:
  assert 'Dos' in diagnosis.stdout,diagnosis.stdout
 assert not marker.exists(),(label,'evaluator ran after failed restore')
 assert 'Restored session ended:' not in text
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
assert hashlib.sha256(source.read_bytes()).hexdigest()==original
print(w,('changed loaded immutable ROM bytes' if os.environ.get('DOSBOX_TEST_CHANGED_BASIC_ROM') else 'malformed Pic/late Keyboard, late CRC settlement and missing late owner')+
      ('/truncated CPU and Memory' if os.environ.get('DOSBOX_TEST_CORE_TRUNCATION') else '')+
      ('/DBOPL volume-wave-synth roles/LFO scalar bounds' if os.environ.get('DOSBOX_TEST_DBOPL') else '')+
      ('/active native search tag/path/truncation' if os.environ.get('DOSBOX_TEST_SEARCH_STATE') else '')+
      ('/native DOS file reopen' if os.environ.get('DOSBOX_TEST_FILE_STATE') else '')+
      ('/BIOS-shell-XMS-EMS bindings/native shell-XMS-EMS bodies' if os.environ.get('DOSBOX_TEST_BINDINGS') else '')+
      ('/PSP signature/MCB ownership/parent/stack/vector' if os.environ.get('DOSBOX_TEST_ROOT_VALIDATION') else '')+
      ('/representation-build-config metadata' if os.environ.get('DOSBOX_TEST_METADATA') else '')+
      ' reject/exit1; source and attempt archives unchanged')

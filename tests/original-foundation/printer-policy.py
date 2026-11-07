#!/usr/bin/env python3
"""F1 real fresh consumers: host policy varies, glyph bytes/device do not."""
import hashlib, os, pathlib, re, subprocess, tempfile
here=pathlib.Path(__file__).resolve().parent
w=pathlib.Path(tempfile.mkdtemp(prefix='printer-policy-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(w,flush=True)
text=(here.parents[1]/'include/DOSBoxTTF.h').read_text()
array=text.split('unsigned char DOSBoxTTFbi[48868]',1)[1].split('{',1)[1].split('}',1)[0]
font=bytes(int(n,16) for n in re.findall(r'0x([0-9a-fA-F]{2})',array))
fonts=w/'fonts';fonts.mkdir()
for name in ('roman','sansserif','courier','script','ocra'):(fonts/(name+'.ttf')).write_bytes(font)
def run(script,env,label):
 p=subprocess.run(['python3',str(here/script)],env=env,capture_output=True,text=True)
 (w/(label+'.log')).write_text(p.stdout+p.stderr)
 assert p.returncode==0,(label,p.stdout,p.stderr)
 return p.stdout
for mode in ('disabled','0','1','2','3'):
 source=w/(mode+'-source');source.mkdir()
 dest=w/(mode+'-dest');dest.mkdir()
 env=dict(os.environ,DOSBOX_TEST_PRINTER_SOURCE_OUTPUT=str(source),DOSBOX_TEST_PRINTER_POLICY=f'docpath={source}\nfontpath={fonts}\nopenwith=\nopenerror=')
 if mode!='disabled':env.update(DOSBOX_TEST_PRINTER=mode,DOSBOX_TEST_CAPTURE_IP='0814:0200',DOSBOX_TEST_REFUSAL_REGISTERS='CS:IP=0814:0201')
 else:env.update(DOSBOX_TEST_PROGRAM_HEX='90 b8 2a 4c cd 21')
 base=pathlib.Path(run('stopped-save.py',env,mode+'-producer').splitlines()[0])
 archive=base/'first.sav';before=hashlib.sha256(archive.read_bytes()).hexdigest()
 source_pages={p.name:p.read_bytes() for p in source.glob('*.ps')}
 env.update(DOSBOX_TEST_ARCHIVE=str(archive),DOSBOX_TEST_ROOT_EXIT='1',DOSBOX_TEST_ROOT_FACTS='code=42 resident=0 self-parent=0',DOSBOX_TEST_PRINTER_POLICY=f'docpath={dest}\nfontpath={fonts}\nopenwith=/bin/true\nopenerror=/bin/false\nshellhide=true\ndevice=unused-host-device')
 if mode!='disabled':env['DOSBOX_TEST_STEP_IP']='0814:0201'
 run('fresh-startup.py',env,mode+'-consumer')
 assert source_pages=={p.name:p.read_bytes() for p in source.glob('*.ps')}
 assert bool(list(dest.glob('*.ps')))==(mode in ('0','3'))
 # Real effective device mismatch must still reject before evaluator use.
 bad=dict(env,DOSBOX_TEST_PRINTER_POLICY=env['DOSBOX_TEST_PRINTER_POLICY']+'\ndpi=180',DOSBOX_TEST_RESTORE_REFUSAL='1',DOSBOX_TEST_FILELPT_REFUSAL='binding')
 run('fresh-startup.py',bad,mode+'-device-refusal')
 if mode=='0':
  # Same configured path, different successfully loaded SFNT bytes. A valid
  # name-table edit retains glyph validity, yet must change resource identity.
  changed=bytearray(font)
  # Edit a byte inside the SFNT name table (not glyph data or offsets).
  import struct
  tables=struct.unpack_from('>H',font,4)[0]
  offset=next(struct.unpack_from('>I',font,12+i*16+8)[0]
              for i in range(tables) if font[12+i*16:16+i*16]==b'name')
  strings=struct.unpack_from('>H',font,offset+4)[0]
  changed[offset+strings+1]^=1
  (fonts/'roman.ttf').write_bytes(changed)
  bad=dict(env,DOSBOX_TEST_RESTORE_REFUSAL='1',DOSBOX_TEST_FILELPT_REFUSAL='binding')
  run('fresh-startup.py',bad,'consumed-font-refusal')
  (fonts/'roman.ttf').write_bytes(font)
 assert hashlib.sha256(archive.read_bytes()).hexdigest()==before
 print(mode,'different host policy fresh restore, held invariance, explicit continuation; device mismatch refused PASS',flush=True)

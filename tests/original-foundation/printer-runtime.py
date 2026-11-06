#!/usr/bin/env python3
"""Real DOS LPT consumer with empty/missing font path and repeated ESC refresh."""
import os,pathlib,re,subprocess,tempfile
w=pathlib.Path(tempfile.mkdtemp(prefix='printer-runtime-',dir=os.environ['SAIVAGE_CARD_WORK_ROOT']))
print(w,flush=True)
here=pathlib.Path(__file__).resolve().parent
asm='''bits 16
org 100h
mov dx,name
mov ax,3d01h
int 21h
jc fail
mov bx,ax
mov si,16
again:
push si
mov dx,text
mov cx,endtext-text
mov ah,40h
int 21h
jc fail
pop si
dec si
jnz again
mov dx,eject
mov cx,1
mov ah,40h
int 21h
jc fail
mov ah,3eh
int 21h
mov dx,witness
xor cx,cx
mov ah,3ch
int 21h
jc fail
mov bx,ax
mov ah,3eh
int 21h
mov ax,4c2ah
int 21h
fail: mov ax,4c63h
int 21h
name db 'LPT1',0
witness db 'PASS.DAT',0
text db 27,'@','Font lifetime ',27,'k',1,'Sans',27,'k',2,'Courier'
 db 27,'k',4,'Script',27,'k',5,'OCRB',27,'k',6,'OCRA',27,'k',0,'Roman',13,10
endtext:
eject db 12
'''
(w/'probe.asm').write_text(asm)
subprocess.run(['nasm','-f','bin',str(w/'probe.asm'),'-o',str(w/'PRINT.COM')],check=True)
# Deterministic real face; no installed host font required for explicit path.
embedded=(here.parents[1]/'include/DOSBoxTTF.h').read_text()
array=embedded.split('unsigned char DOSBoxTTFbi[48868]',1)[1].split('{',1)[1].split('}',1)[0]
font=bytes(int(n,16) for n in re.findall(r'0x([0-9a-fA-F]{2})',array))
assert len(font)==48868
valid=w/'valid-fonts';valid.mkdir()
invalid=w/'invalid-fonts';invalid.mkdir()
for name in ['roman.ttf','sansserif.ttf','courier.ttf','script.ttf','ocra.ttf']:
 (valid/name).write_bytes(font)
 (invalid/name).write_bytes(b'not a font')
for label,path,output in [('empty','','surface'),('missing',str(w/'absent-fonts'),'surface'),('valid',str(valid),'surface'),('invalid',str(invalid),'surface'),('ttf','','ttf')]:
 out=w/label;out.mkdir()
 conf=out/'fixture.conf'
 conf.write_text(f'[sdl]\noutput={output}\n[midi]\nmididevice=none\n[mixer]\nnosound=true\n[parallel]\nparallel1=printer\n[printer]\nprinter=true\nprintoutput=ps\nfontpath={path}\ndocpath={out}\nmultipage=true\n[autoexec]\nmount c {w}\nc:\nPRINT.COM\nexit\n')
 p=subprocess.run([os.environ['DOSBOX_TEST_NATIVE_BINARY'],'-conf',str(conf)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),capture_output=True,text=True,timeout=60)
 (out/'runtime.log').write_text(p.stdout+p.stderr)
 assert p.returncode==0,(out,p.returncode)
 assert (w/'PASS.DAT').is_file(), ('guest did not complete',out)
 (w/'PASS.DAT').unlink()
 pages=list(out.glob('*.ps'));assert pages,(out,p.stdout+p.stderr)
 assert any(page.read_bytes().startswith(b'%!PS') and b'%%EOF' in page.read_bytes() and len(page.read_bytes())>1000 for page in pages),pages
 print(label,'DOS LPT/ESC refresh/close/PS output PASS',flush=True)

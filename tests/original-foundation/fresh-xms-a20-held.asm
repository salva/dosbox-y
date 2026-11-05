; Native XMS local/global A20 accounting must survive independently of gate RAM.
bits 16
org 100h
 nop
 mov ax,4310h
 int 2fh
 mov [entry],bx
 mov [entry+2],es
 ; Start from a disabled gate, then hold two local enables and one global.
 mov ah,4
 call far [entry]
 mov ah,6
 call far [entry]
 mov ah,3
 call far [entry]
 mov ah,5
 call far [entry]
 mov ah,5
 call far [entry]
 jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint:
 nop
 mov ah,6
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,7
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,4
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,7
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,6
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,7
 call far [entry]
 cmp ax,0
 jne failed
 mov ax,4c2ah
 int 21h
failed:
 mov ax,4c63h
 int 21h
entry dd 0

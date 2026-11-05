; Keep an allocated UMB across capture; reconstructed XMS must release it
; without changing DOS's saved conventional/upper MCB allocation strategy.
bits 16
org 100h
 nop
 mov ax,4310h
 int 2fh
 mov [entry],bx
 mov [entry+2],es
 mov ah,10h
 mov dx,20h
 call far [entry]
 cmp ax,1
 jne failed
 mov [umb_segment],bx
 mov es,bx
 mov word [es:0],0a55ah
 jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint:
 nop
 mov es,[umb_segment]
 cmp word [es:0],0a55ah
 jne failed
 mov dx,[umb_segment]
 mov ah,11h
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,10h
 mov dx,20h
 call far [entry]
 cmp ax,1
 jne failed
 mov dx,bx
 mov ah,11h
 call far [entry]
 cmp ax,1
 jne failed
 mov ax,4c2ah
 int 21h
failed:
 mov ax,4c63h
 int 21h
entry dd 0
umb_segment dw 0

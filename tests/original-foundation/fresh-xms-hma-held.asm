; XMS HMA application reservation is native service state, not RAM.
bits 16
org 100h
 nop
 mov ax,4310h
 int 2fh
 mov [entry],bx
 mov [entry+2],es
 mov ah,1
 mov dx,0ffffh
 call far [entry]
 cmp ax,1
 jne failed
 jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint:
 nop
 mov ah,1
 mov dx,0ffffh
 call far [entry]
 cmp ax,0
 jne failed
 cmp bl,91h
 jne failed
 mov ah,2
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,1
 mov dx,0ffffh
 call far [entry]
 cmp ax,1
 jne failed
 mov ah,2
 call far [entry]
 cmp ax,1
 jne failed
 mov ax,4c2ah
 int 21h
failed:
 mov ax,4c63h
 int 21h
entry dd 0

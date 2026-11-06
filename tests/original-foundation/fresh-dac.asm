; Real guest full-load DAC partial triplet, capture before final component.
bits 16
org 100h
start:
%ifdef GRAPHICS
 mov ax,13h
 int 10h
 mov ax,0a000h
 mov es,ax
 mov byte [es:123],5ah
 mov al,[es:123]
 cmp al,5ah
 jne fail
%endif
 mov dx,3c8h
 mov al,9
 out dx,al
 inc dx
 mov al,41
 out dx,al
 mov al,42
 out dx,al
 jmp held
 times 100h-($-start) db 90h
held:
 nop
%ifdef GRAPHICS
 cmp byte [es:123],5ah
 jne fail
 mov byte [es:124],6bh
 cmp byte [es:124],6bh
 jne fail
%endif
 mov al,43
 out dx,al
 mov dx,3c7h
 mov al,9
 out dx,al
 mov dx,3c9h
 in al,dx
 cmp al,41
 jne fail
 in al,dx
 cmp al,42
 jne fail
 in al,dx
 cmp al,43
 jne fail
 mov ax,3
 int 10h
 mov ax,4c2ah
 int 21h
fail:
 mov ax,4c63h
 int 21h

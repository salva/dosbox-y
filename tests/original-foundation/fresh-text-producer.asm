; Real guest 80-column text -> SMZX -> panned ordinary text continuation.
bits 16
org 100h
start:
 mov ax,3
 int 10h
 mov ax,0b800h
 mov es,ax
 xor di,di
 mov cx,2000
 mov ax,1e41h
 rep stosw
 ; Attribute flip-flop reset, select mode control, set text pixel-width bit.
 mov dx,3dah
 in al,dx
 mov dx,3c0h
 mov al,30h
 out dx,al
 inc dx
 in al,dx
 or al,40h
 dec dx
 out dx,al
 ; Run enough instructions for actual live scan producers before capture.
 mov cx,0ffffh
spin: loop spin
 jmp held
 times 100h-($-start) db 90h
held:
 nop
 ; Return to ordinary text, then apply nonzero horizontal panning.
 mov dx,3dah
 in al,dx
 mov dx,3c0h
 mov al,30h
 out dx,al
 inc dx
 in al,dx
 and al,0bfh
 dec dx
 out dx,al
 mov dx,3dah
 in al,dx
 mov dx,3c0h
 mov al,33h
 out dx,al
 mov al,3
 out dx,al
 mov cx,0ffffh
again: loop again
 cmp word [es:0],1e41h
 jne fail
 mov ax,3
 int 10h
 mov ax,4c2ah
 int 21h
fail: mov ax,4c63h
 int 21h

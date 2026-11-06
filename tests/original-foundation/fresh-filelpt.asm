bits 16
org 100h
start:
%ifndef PREFIX
%define PREFIX 0
%endif
 mov cx,PREFIX
 jcxz prefix_done
prefix_loop:
 mov dx,378h
 mov al,'A'
 out dx,al
 mov dx,37ah
 mov al,1
 out dx,al
 xor al,al
 out dx,al
 loop prefix_loop
prefix_done:
 mov dx,378h
 mov al,13
 out dx,al
 mov dx,37ah
 mov al,1
 out dx,al
 xor al,al
 out dx,al
 jmp held
 times 100h-($-start) db 90h
held:
 nop
 mov dx,379h
 in al,dx
 test al,40h
 jnz fail
 in al,dx
 test al,40h
 jz fail
 mov dx,378h
 mov al,'X'
 out dx,al
 mov dx,37ah
 mov al,1
 out dx,al
 xor al,al
 out dx,al
 mov ax,4c2ah
 int 21h
fail:
 mov ax,4c63h
 int 21h

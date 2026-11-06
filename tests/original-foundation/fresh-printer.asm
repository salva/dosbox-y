; MODE 0 never used; 1 partial page; 2 partial ESC; 3 completed page.
bits 16
org 100h
%macro emit 0
 out dx,al
 inc dx
 inc dx
 mov al,5
 out dx,al
 mov al,4
 out dx,al
 sub dx,2
%endmacro
%if MODE != 0
 mov dx,378h
 mov al,'A'
%if MODE == 2
 mov al,27
%endif
 emit
%if MODE == 3
 mov al,12
 emit
%endif
%endif
 jmp held
times 100h-($-$$) db 90h
held:
 nop
 ; Either source continuation (healthy) or restored target (faulted).
 mov dx,379h
 in al,dx
%if MODE == 1 || MODE == 2
 test al,8
 jnz source
 test al,10h
 jnz fail
 ; BIOS AH=2 must report the same low /ERROR hardware bit.
 mov dx,0
 mov ah,2
 int 17h
 test ah,8
 jz fail
 ; DOS LPT device write must fail, not claim the character was printed.
 mov dx,lpt
 mov ax,3d01h
 int 21h
 jc fail
 mov bx,ax
 mov dx,msg
 mov cx,1
 mov ah,40h
 int 21h
 jnc fail
 mov ah,3eh
 int 21h
 jmp success
source:
 ; Prove capture did not fault the running source; complete the operation.
 mov dx,378h
%if MODE == 2
 mov al,'@'
 emit
 mov al,'B'
 emit
%endif
 mov al,12
 emit
%else
 test al,8
 jz fail
 mov dx,378h
 mov al,'Z'
 emit
 mov al,12
 emit
%endif
success:
 mov ax,4c2ah
 int 21h
fail:
 mov ax,4c63h
 int 21h
lpt db 'LPT1',0
msg db 'X'

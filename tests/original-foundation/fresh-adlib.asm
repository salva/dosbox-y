; Active two-operator OPL tone, capture after write-density history.
bits 16
org 100h
start:
 mov si,registers
 mov cx,9
next:
 lodsb
 mov dx,388h
 out dx,al
 inc dx
 lodsb
 out dx,al
 loop next
 ; Trigger the actual optional PCM detection threshold with real data writes.
 mov dx,388h
 mov al,40h
 out dx,al
 inc dx
 mov cx,1100
 mov al,12h
writes:
 out dx,al
 loop writes
 jmp held
 times 100h-($-start) db 90h
held: ; CS:0200; explicit continuation waits for actual mixer/PIC progress.
 nop
 xor ax,ax
 mov es,ax
 mov bx,[es:46ch]
 add bx,2
wait_tick:
 cmp [es:46ch],bx
 jb wait_tick
 mov dx,388h
 mov al,0b0h
 out dx,al
 inc dx
 mov al,11h
 out dx,al
 mov ax,4c2ah
 int 21h
registers: db 20h,1,23h,1,40h,0,43h,0,60h,0f0h,63h,0f0h,0c0h,1,0a0h,98h,0b0h,31h

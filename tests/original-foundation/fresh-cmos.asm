; Active RTC binary/24h locked leap-day clock, alarm, selected register and NMI.
; Capture at 0814:0200 before the reads; no setup replay on destination.
bits 16
org 100h
start:
 nop
 mov al,8bh
 out 70h,al
 mov al,86h ; SET + binary + 24h
 out 71h,al
 mov si,values
 mov cx,9
setup:
 lodsb
 out 70h,al
 lodsb
 out 71h,al
 loop setup
 mov al,89h
 out 70h,al
 jmp held
 values: db 80h,57,82h,58,84h,23,86h,4,87h,29,88h,2,89h,24,81h,17,83h,18
 times 100h-($-start) db 90h
held:
 nop
 in al,71h
 cmp al,24
 jne failed
 mov si,values
 mov cx,9
check:
 lodsb
 out 70h,al
 in al,71h
 cmp al,[si]
 jne failed
 inc si
 loop check
 mov al,8bh
 out 70h,al
 in al,71h
 cmp al,86h
 jne failed
 mov ax,4c2ah
 int 21h
failed:
 mov ax,4c63h
 int 21h

bits 16
org 100h
; Ordinary upstream slot witness. Keep file, memory, timer, input and video live.
mov ax,0013h
int 10h
mov dx,filename
mov cx,0
mov ah,3ch
int 21h
jc fail
mov [handle],ax
mov ax,0a000h
mov es,ax
xor di,di
loop_next:
inc word [counter]
mov ax,[counter]
stosb
mov ah,0
int 1ah
mov [ticks],dx
mov ah,1
int 16h
jz no_key
mov ah,0
int 16h
mov [key],ax
cmp al,27
je finish
no_key:
; Vary speaker divisor using native PIT programming.
mov al,0b6h
out 43h,al
mov ax,[counter]
or ax,100h
out 42h,al
mov al,ah
out 42h,al
in al,61h
or al,3
out 61h,al
mov bx,[handle]
mov dx,counter
mov cx,6
mov ah,40h
int 21h
mov cx,2000
spin: loop spin
jmp loop_next
finish:
mov bx,[handle]
mov ah,3eh
int 21h
in al,61h
and al,0fch
out 61h,al
mov ax,3
int 10h
mov ax,4c00h
int 21h
fail:
mov ax,4c01h
int 21h
filename db 'LIVE.BIN',0
handle dw 0
counter dw 0
ticks dw 0
key dw 0

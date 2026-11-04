; Saved native DOS duplicated handles must share one reopened cursor.
; Producer captures at offset3; host content changes without rollback.
bits 16
org 100h
    nop
    mov dx,filename
    mov ax,3d00h
    int 21h
    jc failed
    mov [first],ax
    mov bx,ax
    mov ax,4200h
    xor cx,cx
    mov dx,3
    int 21h
    jc failed
    mov bx,[first]
    mov ah,45h
    int 21h
    jc failed
    mov [second],ax
    mov bx,[first]
    mov ax,5700h
    int 21h
    jc failed
    mov [saved_time],cx
    mov [saved_date],dx
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint: ; IP0200
    mov bx,[first]
    mov dx,payload
    mov cx,4
    mov ah,3fh
    int 21h
    jc failed
    cmp ax,4
    jne failed
    cmp word [payload],676eh ; ng
    jne failed
    cmp word [payload+2],6465h ; ed
    jne failed
    mov bx,[second]
    mov dx,payload
    mov cx,2
    mov ah,3fh
    int 21h
    jc failed
    cmp ax,2
    jne failed
    cmp word [payload],6220h ; shared cursor7: space,b
    jne failed
    mov bx,[first]
    mov ax,4201h
    xor cx,cx
    xor dx,dx
    int 21h
    jc failed
    cmp ax,9
    jne failed
    ; Closing one guest reference must not invalidate the other.
    mov bx,[first]
    mov ah,3eh
    int 21h
    jc failed
    mov bx,[second]
    mov dx,payload
    mov cx,1
    mov ah,3fh
    int 21h
    jc failed
    cmp ax,1
    jne failed
    cmp byte [payload],'e'
    jne failed
    mov bx,[second]
    mov ax,5700h
    int 21h
    jc failed
    ; AH57 refreshes from current host by normal DOS semantics. It must work
    ; after alias close; equality to capture time would wrongly demand rollback.
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
first dw 0
second dw 0
saved_time dw 0
saved_date dw 0
payload times 4 db 0
filename db 'FLAG.TXT',0

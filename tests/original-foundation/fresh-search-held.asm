; Capture after FindFirst with an active native directory search.
; Consume the remaining entry after restore; root99 detects duplicate/skip.
bits 16
org 100h
%ifndef EXTRA_CURRENT_ENTRY
%define EXTRA_CURRENT_ENTRY 0
%endif
%ifndef DELETE_CURRENT_ENTRY
%define DELETE_CURRENT_ENTRY 0
%endif
    nop
    mov dx,dta
    mov ah,1ah
    int 21h
    mov dx,pattern
    xor cx,cx
    mov ah,4eh
    int 21h
    jc failed
    mov si,dta+30
    mov di,first
    mov cx,13
    rep movsb
    ; A simultaneous DTA search must retain its own native slot.
    mov dx,dta2
    mov ah,1ah
    int 21h
    mov dx,pattern
    xor cx,cx
    mov ah,4eh
    int 21h
    jc failed
    mov dx,dta
    mov ah,1ah
    int 21h
    jmp checkpoint
 times 100h-($-$$) db 90h
checkpoint: ; IP0200
    mov ah,4fh
    int 21h
%if DELETE_CURRENT_ENTRY
    ; Original matching entries were removed between processes. Expose current
    ; exhaustion, without replaying the saved results or failing installation.
    jnc failed
    cmp ax,18
    jne failed
    mov dx,dta2
    mov ah,1ah
    int 21h
    mov ah,4fh
    int 21h
    jnc failed
    cmp ax,18
    jne failed
    mov ax,4c2ah
    int 21h
%endif
    jc failed
    mov si,dta+30
    mov di,first
    mov cx,13
    repe cmpsb
    je failed
%if EXTRA_CURRENT_ENTRY
    ; A new matching host entry is visible in each rebuilt native search.
    mov ah,4fh
    int 21h
    jc failed
    mov si,dta+30
    mov di,added
    mov cx,8
    repe cmpsb
    jne failed
%endif
    ; These probes create exactly ONE.DAT and TWO.DAT. Distinct results plus
    ; truthful exhaustion detect replay of the first result or lost cursor.
    mov ah,4fh
    int 21h
    jnc failed
    cmp ax,18
    jne failed
    mov dx,dta2
    mov ah,1ah
    int 21h
    mov ah,4fh
    int 21h
    jc failed
    mov si,dta2+30
    mov di,first
    mov cx,13
    repe cmpsb
    je failed
%if EXTRA_CURRENT_ENTRY
    ; A new matching host entry is visible in each rebuilt native search.
    mov ah,4fh
    int 21h
    jc failed
    mov si,dta2+30
    mov di,added
    mov cx,8
    repe cmpsb
    jne failed
%endif
    mov ah,4fh
    int 21h
    jnc failed
    cmp ax,18
    jne failed
    mov ax,4c2ah
    int 21h
failed:
    mov ax,4c63h
    int 21h
pattern db '*.DAT',0
added db 'ZZZ.DAT',0
first times 13 db 0
dta times 128 db 0
dta2 times 128 db 0

; Real DOS handle/cursor/search witness for machine-file-state.py
bits 16
org 100h
start:
    mov dx, filename
    mov ax, 3d00h
    int 21h
    jc fail
    mov [handle], ax
    mov bx, ax
    mov ax, 4200h
    xor cx, cx
    mov dx, 3
    int 21h
    jc fail
    jmp checkpoint
fail:
    mov word [result], 0ffffh
    jmp done
 times 80h-($-$$) db 90h
checkpoint: ; IP 0180, held with open file at offset 3
    mov bx, [handle]
    mov dx, payload
    mov cx, 4
    mov ah, 3fh
    int 21h
    jc fail
    mov [count], ax
    mov bx, [handle]
    mov ax, 4201h
    xor cx, cx
    xor dx, dx
    int 21h
    jc fail
    mov [cursor], ax
    ; A restored read-only handle must still refuse writes.
    mov bx, [handle]
    mov dx, payload
    mov cx, 1
    mov ah, 40h
    int 21h
    jnc fail
    mov [error], ax
    ; Start a new enumeration of the current host directory after restore.
    ; Persisting an already active native search is separate parent owner work.
    mov dx, dta
    mov ah, 1ah
    int 21h
    mov dx, pattern
    xor cx, cx
    mov ah, 4eh
    int 21h
    jc fail
    mov si, dta+30
    mov di, firstname
    mov cx, 13
    rep movsb
    mov ah, 4fh
    int 21h
    jc fail
    mov si, dta+30
    mov di, nextname
    mov cx, 13
    rep movsb
    ; Exhaust the two-entry search: truthful DOS no-more-files error.
    mov ah, 4fh
    int 21h
    jnc fail
    mov [search_error], ax
    mov word [result], 1234h
    jmp done
 times 108h-($-start) db 90h
done: jmp done ; IP 0208
 times 110h-($-start) db 0
 db 'R3-FILE-WITNESS!'
result: dw 0
count: dw 0
cursor: dw 0
error: dw 0
search_error: dw 0
payload: times 4 db 0
nextname: times 13 db 0
firstname: times 13 db 0
handle: dw 0
filename: db 'FLAG.TXT',0
pattern: db '*.DAT',0
dta: times 128 db 0

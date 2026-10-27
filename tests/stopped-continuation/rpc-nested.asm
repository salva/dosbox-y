; Selected parent remains live after a nested child's INT 21h/4Ch return.
org 100h
    mov sp, stack_top
    mov bx, (stack_top - $ + 100h + 15) / 16
    mov ah, 4ah
    int 21h
    jc failed
    mov ax, cs
    mov [parameter_block + 4], ax
    mov [parameter_block + 8], ax
    mov [parameter_block + 12], ax
    mov dx, child_name
    mov bx, parameter_block
    mov ax, 4b00h
    int 21h
    jc failed
    int 3
    mov ax, 4c00h
    int 21h
failed:
    mov ax, 4c01h
    int 21h
child_name: db 'CHILD.COM',0
parameter_block: dw 0, command_tail, 0, 5ch, 0, 6ch, 0
command_tail: db 0,13
    times 256 db 0
stack_top:

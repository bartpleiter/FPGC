; Stack overflow: a push when SP is 255. It sets the stack-overflow flag (kind 5), and
; SP wraps around to 0. After the first case, the program reads ERR_STATUS and ERR_PC,
; and clears them.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load 255 r12
Fill:
    push r12
    sub r12 1 r12
    bne r12 r0 Fill         ; 255 pushes, so SP is 255
    read 4 r10 r1           ; SP: 255
    push r1                 ; the 256th push overflows
    read 4 r10 r1           ; SP: 0
    read 8 r10 r2           ; ERR_STATUS: 0x510
    read 12 r10 r3          ; ERR_PC: the address of the push
    write 8 r10 r0

    load 255 r1
    write 4 r10 r1          ; SP = 255
    push r1
    read 8 r10 r15 ; expected=1296
    halt

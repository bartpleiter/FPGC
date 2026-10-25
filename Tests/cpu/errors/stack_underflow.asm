; Stack underflow: a pop when SP is 0. It sets the stack-underflow flag (kind 6), SP
; wraps around to 255, and the pop gets the word at 255. After the first case, the
; program reads ERR_STATUS and ERR_PC, and clears them.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    pop r1                  ; 0, as the stack starts at 0
    read 4 r10 r2           ; SP: 255
    read 8 r10 r3           ; ERR_STATUS: 0x620
    read 12 r10 r4          ; ERR_PC: the address of the pop
    write 8 r10 r0

    write 4 r10 r0          ; SP = 0
    pop r0                  ; also pops
    read 8 r10 r15 ; expected=1568
    halt

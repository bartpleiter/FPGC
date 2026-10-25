; The CPU-internal I/O registers: PC_BACKUP can be written and read back, SP follows
; pushes, pops and writes, and the error register is 0 when nothing went wrong.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load32 0x12345678 r1
    write 0 r10 r1          ; PC_BACKUP
    read 0 r10 r2           ; 0x12345678
    push r1
    push r2
    read 4 r10 r3           ; SP: 2
    load 1 r4
    write 4 r10 r4          ; SP = 1, which drops the second word
    pop r5                  ; the first word: 0x12345678
    load32 0x103 r4
    write 4 r10 r4          ; SP = 3, as only bits [7:0] count
    read 4 r10 r6           ; 3
    write 4 r10 r0          ; SP = 0, so the stack is empty again
    read 8 r10 r7           ; ERR_STATUS: 0
    read 12 r10 r8          ; ERR_PC: 0
    add r5 r6 r15 ; expected=305419899
    halt

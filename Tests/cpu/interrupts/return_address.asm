; The handler changes PC_BACKUP directly before RETI, so RETI returns somewhere else
; (as a task switch would). RETI must see that write.
; interrupt 5 at cycle 20
; interrupts taken: 5
    jump Main
    jump Interrupt
Interrupt:
    load32 0x1F000000 r2
    addr2reg Elsewhere r3
    write 0 r2 r3           ; PC_BACKUP
    reti
Main:
    load 1 r6
Wait:
    beq r6 r6 Wait          ; until the interrupt
Elsewhere:
    load 2 r6
    halt

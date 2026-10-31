; An interrupt during a loop. It's taken instead of the branch back, the handler reads
; which interrupt it was and where it came from, and after RETI the branch runs as if
; nothing happened, so the loop still runs 20 times.
; interrupt 2 at cycle 30
; interrupts taken: 2
    jump Main
    jump Interrupt
Interrupt:
    readintid r1            ; 2
    load32 0x1F000000 r2
    read 0 r2 r3            ; PC_BACKUP: the address of the branch
    reti
Main:
    load 20 r5
Loop:
    add r6 1 r6
    sub r5 1 r5
    bne r5 r0 Loop
    halt

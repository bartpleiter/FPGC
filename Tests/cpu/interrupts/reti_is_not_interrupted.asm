; RETI is never replaced by an interrupt, also when interrupts are enabled and one is
; pending. Here the program uses RETI as a jump, after the divides have given the
; interrupt time to become pending. The interrupt is taken at the jump after it.
; interrupt 4 at cycle 30
; interrupts taken: 4
    jump Main
    jump Interrupt
Interrupt:
    readintid r1            ; 4
    reti
Main:
    load32 0x1F000000 r2
    addr2reg Next r3
    write 0 r2 r3           ; PC_BACKUP
    load 100 r4
    divu r4 r4 r5
    divu r4 r5 r5
    divu r4 r5 r5
    reti                    ; to Next
Next:
    jump Done
Done:
    halt

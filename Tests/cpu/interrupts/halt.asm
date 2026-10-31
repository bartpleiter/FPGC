; HALT is a jump to itself, so it can be interrupted: a program can wait there for an
; interrupt. The divides take long enough for the interrupt to be pending by the time
; the HALT is reached. After RETI, the HALT runs again.
; interrupt 7 at cycle 50
; interrupts taken: 7
    jump Main
    jump Interrupt
Interrupt:
    readintid r1            ; 7
    reti
Main:
    load 100 r2
    divu r2 r2 r3
    divu r2 r3 r3
    divu r2 r3 r3
    halt

; An interrupt that comes during the handler waits, even when it has a lower number:
; interrupts don't nest. It's taken right after RETI, at the HALT it returns to.
; interrupt 3 at cycle 40
; interrupt 1 at cycle 250
; interrupts taken: 3 1
    jump Main
    jump Interrupt
Interrupt:
    readintid r1
    load 100 r4
    divu r4 r4 r5
    divu r4 r5 r5
    divu r4 r5 r5
    divu r4 r5 r5
    reti
Main:
    load 100 r4
    divu r4 r4 r5
    divu r4 r5 r5
    divu r4 r5 r5
    divu r4 r5 r5
    halt

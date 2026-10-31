; The CPU takes the lowest number that is pending when the jump leaves MEM, not the one
; that was pending first. Interrupt 5 comes first, but 1 comes before the next jump, so
; 1 is taken first, and 5 right after its RETI.
; interrupt 5 at cycle 40
; interrupt 1 at cycle 80
; interrupts taken: 1 5
    jump Main
    jump Interrupt
Interrupt:
    readintid r1
    reti
Main:
    load 100 r4
    divu r4 r4 r5
    divu r4 r5 r5
    divu r4 r5 r5
    divu r4 r5 r5
    halt

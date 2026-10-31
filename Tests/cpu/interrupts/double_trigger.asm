; Two edges on the same input before its interrupt is taken count as one, as there is
; one pending bit per input. Once it's taken, a new edge counts again, also during the
; handler: that one is taken right after RETI, at the HALT it returns to. The divides
; keep the CPU away from jumps, so the first two edges come before the first take.
; interrupt 2 at cycle 40
; interrupt 2 at cycle 50
; interrupt 2 at cycle 250
; interrupts taken: 2 2
    jump Main
    jump Interrupt
Interrupt:
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

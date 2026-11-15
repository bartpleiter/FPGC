; Three interrupts at the same time are taken one at a time, the lowest number first,
; at the branch of the loop. The handler shifts each number into r7, so r7 ends as
; 0x123. Its own taken branch is not interrupted, because interrupts are disabled until
; RETI. The interrupts come once the loop runs, with every cache size and latency, so
; that r7 is written before the handler reads it.
; interrupt 3 at cycle 400
; interrupt 1 at cycle 400
; interrupt 2 at cycle 400
; interrupts taken: 1 2 3
    jump Main
    jump Interrupt
Interrupt:
    readintid r1
    shiftl r7 4 r7
    or r7 r1 r7
    beq r0 r0 Return        ; taken, with interrupts disabled
    load 1 r8               ; skipped
Return:
    reti
Main:
    load 0 r7
    load 300 r5
Loop:
    sub r5 1 r5
    bne r5 r0 Loop
    halt

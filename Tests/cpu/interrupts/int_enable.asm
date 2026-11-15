; INT_ENABLE disables and enables interrupts. Interrupt 3 becomes pending while they are
; disabled, so the branches of the loop aren't interrupted. A write to INT_ENABLE counts
; from the next instruction on: the jump right behind the write that disables them again
; isn't interrupted, and the jump right behind the write that enables them is.
; interrupt 3 at cycle 700
; interrupts taken: 3
    jump Main
    jump Interrupt
Interrupt:
    readintid r5            ; 3
    reti
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load 1 r1
    write 16 r10 r0         ; disable
    load 400 r2
Wait:                       ; the interrupt becomes pending in this loop
    sub r2 1 r2
    bne r2 r0 Wait
    write 16 r10 r1         ; enable, with no jump before the next write
    write 16 r10 r0         ; disable
    jump Disabled           ; not interrupted
Disabled:
    jump Read
Read:
    read 16 r10 r3          ; 0
    write 16 r10 r1         ; enable
Enabled:
    jump Done               ; interrupted
Done:
    read 16 r10 r4          ; 1, after RETI
    halt

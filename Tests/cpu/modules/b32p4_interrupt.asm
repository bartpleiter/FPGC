; Interrupts in the pipeline (fpgc cpu wave b32p4_interrupt).
; The program runs from SDRAM, because code in ROM is never interrupted. Interrupt 3
; comes during the loop, and is taken instead of the branch back: the branch goes
; through the pipeline, but in MEM it jumps to the interrupt vector at address 4
; instead, and pc_backup gets its address. The handler reads the interrupt number, and
; RETI jumps back to the branch, which then runs as normal.
; interrupt 3 at cycle 10
    jump Main
    jump Interrupt
Interrupt:
    readintid r1        ; 3
    reti
Main:
    load 4 r2
Loop:
    sub r2 1 r2
    bne r2 r0 Loop
    halt

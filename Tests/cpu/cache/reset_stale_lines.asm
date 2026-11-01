; A reset in the middle of the run: after it, nothing from before may hit. The reset
; comes in the delay loop, and the program then runs again from the start. The ISS runs
; it once, on a fresh machine, and the comparison is with the run after the reset.
;
; Before the reset, the L1D has a dirty line with a word that SDRAM doesn't have, and
; the L1I has a line with code that SDRAM no longer has:
; - The program reads 0x10000 (0), and at the end writes 0x5A5A there. The walk after
;   the reset doesn't write that line back, so the read after the reset must give 0
;   again.
; - Target first returns with r5 = 1. The program copies NewCode over it, CCACHE, and
;   calls it again, so the L1I holds the new code (r5 = 2). It then copies OldCode back
;   over it and does CCACHED, which writes it back without invalidating the L1I. So
;   after the reset, Target must give r5 = 1 again, from SDRAM.
; runs from SDRAM
; reset at cycle 2000
    load32 0x10000 r10
    read 0 r10 r1           ; 0, also after the reset
    addr2reg Target r2
    addr2reg Return1 r7
    jumpr 0 r2              ; r5 = 1, also after the reset
Return1:
    addr2reg NewCode r1
    savpc r7
    add r7 12 r7            ; to Return2
    jump Copy
Return2:
    ccache                  ; Target has the new code in SDRAM, and no cache has it
    addr2reg Return3 r7
    jumpr 0 r2              ; r5 = 2, and the L1I holds the new code
Return3:
    addr2reg OldCode r1
    savpc r7
    add r7 12 r7            ; to Return4
    jump Copy
Return4:
    ccached                 ; Target has the old code in SDRAM, the L1I the new code
    load 0x5A5A r3
    write 0 r10 r3          ; a dirty line, which is never written back
    load32 1000 r4
Delay:                      ; the reset comes in this loop
    sub r4 1 r4
    bne r4 r0 Delay
    halt

; Copies the two words at r1 to Target, and returns to r7
Copy:
    read 0 r1 r3
    write 0 r2 r3
    read 4 r1 r3
    write 4 r2 r3
    jumpr 0 r7
    nop
    nop
    nop
    nop
    nop
    nop
    nop
Target:
    load 1 r5
    jumpr 0 r7
OldCode:
    load 1 r5
    jumpr 0 r7
NewCode:
    load 2 r5
    jumpr 0 r7

; An interrupt taken instead of a jump with a bad target: the jump doesn't execute, so
; it sets no flag, and the handler reads ERR_STATUS as 0. After RETI the jump executes
; and sets the bad-jump-target flag (kind 4). The divides give the interrupt time to
; become pending before the jump.
; interrupt 1 at cycle 50
    jump Main
    jump Interrupt
Interrupt:
    load32 0x1F000000 r9
    read 8 r9 r1            ; ERR_STATUS: 0
    reti
Main:
    load32 0x1F000000 r10
    load 100 r4
    divu r4 r4 r5
    divu r4 r5 r5
    divu r4 r5 r5
    addr2reg Target r2
    jumpr 2 r2              ; to Target + 2, so to Target
    load 1 r15              ; skipped
Target:
    read 8 r10 r15 ; expected=1032
    halt

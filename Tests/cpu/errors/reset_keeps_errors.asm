; The error register keeps its value over a reset, so that software can see after a
; reset what went wrong before it. The reset comes in the delay loop, and the program
; then runs again from the start: it reads 0x101 and the address of the .dw, which the
; first run left behind.
; reset at cycle 600
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    read 8 r10 r1           ; ERR_STATUS: 0 at the first start, 0x101 after the reset
    read 12 r10 r2          ; ERR_PC: 0, then the address of the .dw
    .dw 0x08000123          ; a reserved ALU operation: kind 1
    load 300 r4
Delay:                      ; the reset comes in this loop
    sub r4 1 r4
    bne r4 r0 Delay
    read 8 r10 r15 ; expected=257
    halt

; Interrupts on the board, a program that the boot ROM loads: the timer interrupts a
; computation every 1,000 cycles, and the handler counts the interrupts. The computation
; (100,000 steps of xorshift32 on r1) must give the same result, however many times it
; was interrupted, and wherever. Prints "result <r1> interrupts <count>", and returns to
; the boot ROM. The handler only uses r10, the count.

    jump Main
    jump Interrupt

Main:
    load 0 r10                  ; the interrupts
    load 3 r1
    write 0xFFFF8204 r0 r1      ; timer CONTROL: periodic, interrupt
    load 1000 r1
    write 0xFFFF8200 r0 r1      ; timer COUNT
    load 1 r1
    write 0xFFFFFF10 r0 r1      ; INT_ENABLE

    load32 0x12345678 r1
    load32 100000 r2            ; STEPS
Step:
    shiftl r1 13 r3
    xor r1 r3 r1
    shiftr r1 17 r3
    xor r1 r3 r1
    shiftl r1 5 r3
    xor r1 r3 r1
    sub r2 1 r2
    bne r2 r0 Step

    write 0xFFFFFF10 r0 r0      ; INT_ENABLE: off
    write 0xFFFF8204 r0 r0      ; timer CONTROL: off
    write 0xFFFF8200 r0 r0      ; timer COUNT: stopped
    or r1 r0 r5
    addr2reg TextResult r1
    savpc 8 r15
    jump PrintText
    or r5 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextInterrupts r1
    savpc 8 r15
    jump PrintText
    or r10 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
    jump 0x04000000             ; back to the boot ROM

Interrupt:
    write 0xFFFF8208 r0 r0      ; timer STATUS: clears the flag
    add r10 1 r10
    reti

; Prints the text at r1, one character per word, up to a 0. Uses r1 and r2.
PrintText:
    read 0 r1 r2
    beq r2 r0 PrintTextDone
    write 0xFFFF8000 r0 r2      ; UART TRANSMIT
    add r1 4 r1
    jump PrintText
PrintTextDone:
    jumpr 0 r15

; Prints r1 as 8 hex digits. Uses r1 to r4.
PrintHex:
    load 8 r3                   ; digits left
    load 10 r4
PrintHexNext:
    shiftr r1 28 r2             ; the highest digit
    shiftl r1 4 r1
    blt r2 r4 PrintHexDigit
    add r2 0x27 r2              ; a to f: 0x61 is 0x30 + 10 + 0x27
PrintHexDigit:
    add r2 0x30 r2
    write 0xFFFF8000 r0 r2      ; UART TRANSMIT
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r15

TextResult:    ; "result "
    .dw 0x72 0x65 0x73 0x75 0x6C 0x74 0x20 0
TextInterrupts: ; " interrupts "
    .dw 0x20 0x69 0x6E 0x74 0x65 0x72 0x72 0x75 0x70 0x74 0x73 0x20 0
TextNewline:   ; "\r\n"
    .dw 0x0D 0x0A 0

; Boot ROM
; Quiets the devices, prints the error register and "boot: ready", and then echoes
; every byte it receives. The loader replaces the echo in B7.
; Functions are called with `savpc 8 r15` and `jump`, and return with `jumpr 0 r15`.

Start:
    write 0xFFFFFF10 r0 r0      ; INT_ENABLE: no interrupts
    write 0xFFFF800C r0 r0      ; UART CONTROL: no interrupt
    write 0xFFFF8204 r0 r0      ; timer CONTROL: one-shot, no interrupt
    write 0xFFFF8200 r0 r0      ; timer COUNT: stopped
    write 0xFFFF8208 r0 r0      ; timer STATUS: flag cleared

    ; boot: err <ERR_STATUS> pc <ERR_PC>
    addr2reg TextErr r1
    savpc 8 r15
    jump PrintText
    read 0xFFFFFF08 r0 r1       ; ERR_STATUS
    savpc 8 r15
    jump PrintHex
    addr2reg TextPc r1
    savpc 8 r15
    jump PrintText
    read 0xFFFFFF0C r0 r1       ; ERR_PC
    savpc 8 r15
    jump PrintHex
    write 0xFFFFFF08 r0 r0      ; clears the error register
    addr2reg TextReady r1
    savpc 8 r15
    jump PrintText

Echo:
    read 0xFFFF8008 r0 r1       ; UART STATUS
    and r1 1 r1                 ; a byte is waiting
    beq r1 r0 Echo
    read 0xFFFF8004 r0 r1       ; RECEIVE
    write 0xFFFF8000 r0 r1      ; TRANSMIT
    jump Echo

; Prints the text at r1, one character per word, up to a 0. Uses r1 and r2.
PrintText:
    read 0 r1 r2
    beq r2 r0 PrintTextDone
    write 0xFFFF8000 r0 r2      ; TRANSMIT
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
    write 0xFFFF8000 r0 r2      ; TRANSMIT
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r15

TextErr:      ; "boot: err "
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x65 0x72 0x72 0x20 0
TextPc:       ; " pc "
    .dw 0x20 0x70 0x63 0x20 0
TextReady:    ; "\r\nboot: ready\r\n"
    .dw 0x0D 0x0A 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x72 0x65 0x61 0x64 0x79 0x0D 0x0A 0

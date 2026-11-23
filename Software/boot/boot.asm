; Boot ROM
; Quiets all devices, prints the error register and "boot: ready", and then loads a
; program over the UART into SDRAM and runs it.
;
; The PC sends "L", the length in bytes (a multiple of 4, at most 64 MiB), the program
; and its CRC-32, each word lowest byte first. The ROM writes the program to SDRAM from
; address 0, and answers "boot: ok <crc>" and runs it, or "boot: bad <crc>" and waits
; again. Other bytes are ignored. A program is allowed to return by jumping to
; the first ROM address as a form of soft-reset.
;
; Functions are called with `savpc 8 r15` and `jump`, and return with `jumpr 0 r15`.
; ReceiveByte is called with r14 instead, so ReceiveWord can call it.

Start:
    write 0xFFFFFF10 r0 r0      ; INT_ENABLE: no interrupts
    write 0xFFFFFF00 r0 r0      ; PC_BACKUP: 0
    write 0xFFFFFF04 r0 r0      ; SP: an empty stack
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
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText

Ready:
    addr2reg TextReady r1
    savpc 8 r15
    jump PrintText

; Waits for the command "L"
Command:
    savpc 8 r14
    jump ReceiveByte
    load 0x4C r2                ; "L"
    bne r1 r2 Command

; The length in r6, a multiple of 4 and at most 64 MiB
    savpc 8 r15
    jump ReceiveWord
    or r3 r0 r6
    and r6 3 r2
    bne r2 r0 BadLength
    load32 0x04000000 r2
    bgt r6 r2 BadLength

; The program: each word to SDRAM at r7, and each byte into the CRC in r5
    load32 0xFFFFFFFF r5
    load32 0xEDB88320 r10       ; the CRC-32's polynomial, bits reversed
    load 0 r7
LoadWord:
    beq r7 r6 LoadDone
    load 0 r3                   ; the word
    load 0 r8                   ; the shift of the next byte
LoadByte:
    savpc 8 r14
    jump ReceiveByte
    shiftl r1 r8 r2
    or r3 r2 r3
    xor r5 r1 r5                ; the CRC, one bit at a time
    load 8 r9
CrcBit:
    and r5 1 r2
    shiftr r5 1 r5
    beq r2 r0 CrcNext
    xor r5 r10 r5
CrcNext:
    sub r9 1 r9
    bne r9 r0 CrcBit
    add r8 8 r8
    load 32 r2
    bne r8 r2 LoadByte
    write 0 r7 r3
    add r7 4 r7
    jump LoadWord

; The PC's CRC in r3, compared with the program's
LoadDone:
    not r5 r5
    savpc 8 r15
    jump ReceiveWord
    bne r3 r5 Bad
    addr2reg TextOk r1
    savpc 8 r15
    jump PrintText
    or r5 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
    ccache                      ; the program to SDRAM, and out of the caches
    jump 0

Bad:
    addr2reg TextBad r1
    savpc 8 r15
    jump PrintText
    or r5 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
    jump Ready

BadLength:
    addr2reg TextBadLength r1
    savpc 8 r15
    jump PrintText
    jump Ready

; Waits for a byte, and returns it in r1. Uses r2. Returns with r14.
ReceiveByte:
    read 0xFFFF8008 r0 r2       ; UART STATUS
    and r2 1 r2                 ; a byte is waiting
    beq r2 r0 ReceiveByte
    read 0xFFFF8004 r0 r1       ; RECEIVE
    jumpr 0 r14

; Receives a word, lowest byte first, into r3. Uses r1, r2, r8 and r14.
ReceiveWord:
    load 0 r3
    load 0 r8
ReceiveWordNext:
    savpc 8 r14
    jump ReceiveByte
    shiftl r1 r8 r1
    or r3 r1 r3
    add r8 8 r8
    load 32 r2
    bne r8 r2 ReceiveWordNext
    jumpr 0 r15

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

TextErr:        ; "boot: err "
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x65 0x72 0x72 0x20 0
TextPc:         ; " pc "
    .dw 0x20 0x70 0x63 0x20 0
TextReady:      ; "boot: ready\r\n"
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x72 0x65 0x61 0x64 0x79 0x0D 0x0A 0
TextOk:         ; "boot: ok "
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x6F 0x6B 0x20 0
TextBad:        ; "boot: bad "
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x62 0x61 0x64 0x20 0
TextBadLength:  ; "boot: bad length\r\n"
    .dw 0x62 0x6F 0x6F 0x74 0x3A 0x20 0x62 0x61 0x64 0x20 0x6C 0x65 0x6E 0x67 0x74 0x68
    .dw 0x0D 0x0A 0
TextNewline:    ; "\r\n"
    .dw 0x0D 0x0A 0

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

.equ PC_BACKUP 0xFFFFFF00
.equ SP 0xFFFFFF04
.equ ERR_STATUS 0xFFFFFF08
.equ ERR_PC 0xFFFFFF0C
.equ INT_ENABLE 0xFFFFFF10
.equ UART_TRANSMIT 0xFFFF8000
.equ UART_RECEIVE 0xFFFF8004
.equ UART_STATUS 0xFFFF8008
.equ UART_CONTROL 0xFFFF800C
.equ TIMER_COUNT 0xFFFF8200
.equ TIMER_CONTROL 0xFFFF8204
.equ TIMER_STATUS 0xFFFF8208
.equ SDRAM_END 0x04000000

Start:
    write INT_ENABLE r0 r0      ; no interrupts
    write PC_BACKUP r0 r0
    write SP r0 r0              ; an empty stack
    write UART_CONTROL r0 r0    ; no interrupt
    write TIMER_CONTROL r0 r0   ; one-shot, no interrupt
    write TIMER_COUNT r0 r0     ; stopped
    write TIMER_STATUS r0 r0    ; the flag cleared

    ; boot: err <ERR_STATUS> pc <ERR_PC>
    addr2reg TextErr r1
    savpc 8 r15
    jump PrintText
    read ERR_STATUS r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextPc r1
    savpc 8 r15
    jump PrintText
    read ERR_PC r0 r1
    savpc 8 r15
    jump PrintHex
    write ERR_STATUS r0 r0      ; clears the error register
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
    load 'L' r2
    bne r1 r2 Command

; The length in r6, a multiple of 4 and at most 64 MiB
    savpc 8 r15
    jump ReceiveWord
    or r3 r0 r6
    and r6 3 r2
    bne r2 r0 BadLength
    load32 SDRAM_END r2
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
    read UART_STATUS r0 r2
    and r2 1 r2                 ; a byte is waiting
    beq r2 r0 ReceiveByte
    read UART_RECEIVE r0 r1
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

; Prints the string at r1, up to its 0. Uses r1 and r2.
PrintText:
    readbu 0 r1 r2
    beq r2 r0 PrintTextDone
    write UART_TRANSMIT r0 r2
    add r1 1 r1
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
    add r2 39 r2                ; a to f: 'a' is '0' + 10 + 39
PrintHexDigit:
    add r2 '0' r2
    write UART_TRANSMIT r0 r2
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r15

TextErr:
    .ds "boot: err "
TextPc:
    .ds " pc "
TextReady:
    .ds "boot: ready\r\n"
TextOk:
    .ds "boot: ok "
TextBad:
    .ds "boot: bad "
TextBadLength:
    .ds "boot: bad length\r\n"
TextNewline:
    .ds "\r\n"

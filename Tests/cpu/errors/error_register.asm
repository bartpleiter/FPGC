; The error register. Instructions that never execute set no flags. When one
; instruction has two events, the lower kind counts as the first. Only the first event
; is recorded in detail. A write to ERR_STATUS clears it before the events of that
; write itself.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load32 0x10000000 r12   ; unmapped

    beq r0 r0 Skip          ; flushes the illegal instructions behind it
    .dw 0x08000123
    .dw 0x08000123
    .dw 0x08000123
Skip:
    read 8 r10 r1           ; ERR_STATUS: 0

    read 2 r12 r1           ; unaligned and unmapped: kinds 2 and 3
    read 8 r10 r2           ; 0x206
    read 12 r10 r3          ; ERR_PC: the address of the read
    pop r1                  ; underflow: kind 6, only its flag
    read 8 r10 r2           ; 0x226
    read 12 r10 r3          ; still the address of the read
    write 10 r10 r0         ; unaligned, at ERR_STATUS: clears, then kind 2
    read 12 r10 r3          ; the address of the write
    read 8 r10 r15 ; expected=514 (0x202)
    halt

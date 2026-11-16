; Bad addresses behind the bus: the device bridge answers an address where nothing is
; with an error, and the CPU sets the bad-address flag (kind 3), as for its own unmapped
; addresses. A bad read gives 0, and a bad write does nothing. The device registers are
; reached from r0 with a negative offset, written as the address itself.
; After each case, the program reads ERR_STATUS and ERR_PC, and clears them.
Main:
    load 5 r1
    write 0xFFFF8010 r0 r1  ; a device register: no error
    read 0xFFFF8010 r0 r2   ; 5
    read 0xFFFFFF08 r0 r3   ; ERR_STATUS: 0

    load32 0x04001000 r12   ; right after the ROM
    read 0 r12 r2           ; 0
    read 0xFFFFFF08 r0 r3   ; 0x304
    read 0xFFFFFF0C r0 r4   ; ERR_PC: the address of the read
    write 0xFFFFFF08 r0 r0

    load32 0x05000000 r12   ; the GPU's region, with nothing yet
    write 0 r12 r1
    read 0xFFFFFF08 r0 r3
    read 0xFFFFFF0C r0 r4
    write 0xFFFFFF08 r0 r0

    load32 0x07FFFFFC r12   ; the last word behind the bus
    readb 3 r12 r2          ; 0
    read 0xFFFFFF08 r0 r3
    read 0xFFFFFF0C r0 r4
    write 0xFFFFFF08 r0 r0

    read 0xFFFFFF14 r0 r2   ; after the CPU-internal registers: the CPU's own error
    read 0xFFFFFF08 r0 r3
    read 0xFFFFFF0C r0 r4
    write 0xFFFFFF08 r0 r0

    read 0xFFFF8010 r0 r2   ; still 5
    read 0xFFFFFF08 r0 r15 ; expected=0
    halt

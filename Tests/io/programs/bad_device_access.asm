; Accesses that the device bridge or a device refuses are bad addresses (kind 3), as
; any other: a byte read of a device register, a register that the device doesn't
; have, and a device that isn't there. A bad read gives 0. After each case, the
; program reads ERR_STATUS and clears it. Then it reads the DIP switches: 0101.
; devices
    load 7 r1
    readb 0xFFFF8300 r0 r2      ; a byte of the board's LEDs
    read 0xFFFFFF08 r0 r3
    write 0xFFFFFF08 r0 r0
    read 0xFFFF8310 r0 r2       ; not a register of the board
    read 0xFFFFFF08 r0 r3
    write 0xFFFFFF08 r0 r0
    write 0xFFFF8A00 r0 r1      ; no device 10
    read 0xFFFFFF08 r0 r3
    write 0xFFFFFF08 r0 r0
    read 0xFFFF8304 r0 r4       ; the switches
    halt

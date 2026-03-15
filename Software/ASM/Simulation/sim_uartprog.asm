; Simple program that fills VRAMPX with a counter, and increments the value each frame
;  creating a moving pattern from right to left.
; Note: In byte-addressable mode, each VRAMPX pixel occupies 4 bytes of address space
;  (BRAM index = (byte_addr - base) >> 2), so addresses must increment by 4.
Main:
    load32 0x1EC00000 r11 ; VRAMPX base address
    load32 76800 r4 ; Number of pixels (320*240)
    shiftl r4 2 r4 ; Convert to byte address range (76800 * 4 = 307200)
    add r11 r4 r11 ; Stop address
    load 0 r2 ; Pixel value
    Start:
        load32 0x1EC00000 r1 ; Start address
        Loop:
            write 0 r1 r2 ; Write pixel value
            add r1 4 r1 ; Next pixel (word-aligned, stride 4)
            add r2 1 r2 ; Increment pixel value
            beq r1 r11 8 ; End loop when we reach the stop address
                jump Loop
    
    add r2 1 r2 ; Increment pixel value for next frame
    jump Start ; Jump back after the initialization

    halt ; Will never reach here

; Ignore interrupts
Int:
    reti

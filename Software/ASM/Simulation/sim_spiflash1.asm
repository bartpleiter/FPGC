; Simple program that fills VRAMPX with a counter, and increments the value each frame
;  creating a moving pattern from right to left.
Main:
    load32 0x7B00000 r11 ; VRAMPX base address
    load32 76800 r4 ; Number of pixels (320*240)
    add r11 r4 r11 ; Stop address
    load 0 r2 ; Pixel value
    Start:
        load32 0x7B00000 r1 ; Start address
        Loop:
            write 0 r1 r2 ; Write pixel value
            add r1 1 r1 ; Next pixel
            add r2 1 r2 ; Increment pixel value
            beq r1 r11 2 ; End loop when we reach the stop address
                jump Loop
    
    add r2 1 r2 ; Increment pixel value for next frame
    jump Start ; Jump back after the initialization

    halt ; Will never reach here

; Ignore interrupts
Int:
    reti

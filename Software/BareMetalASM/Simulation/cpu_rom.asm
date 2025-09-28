Main:

    nop
    load 0 r1 ; SDRAM address counter
    load32 76800 r2 ; amount of pixels to write (320x240)
    
    load 0 r3 ; color value

    SDRAM_Write_Loop:
        write 0 r1 r3 ; write color value to SDRAM
        add r1 1 r1 ; increment SDRAM address
        add r3 1 r3 ; increment color value
        beq r1 r2 2 ; if we wrote all pixels, skip next instruction
        jump 125829125 ; Address of SDRAM_Write_Loop


    nop
    load32 0x7B00000 r4 ; VRAMpx base address and counter
    load32 76800 r5 ; 320*240 pixels
    
    add r4 r5 r5 ; VRAMpx end address
    load 0 r6 ; SDRAM address counter

    VRAM_Copy_Loop:
        read 0 r6 r7 ; read pixel from SDRAM
        write 0 r4 r7 ; write pixel to VRAMpx
        add r4 1 r4 ; increment VRAMpx address
        add r6 1 r6 ; increment SDRAM address
        beq r4 r5 2 ; if we copied all pixels, skip next instruction
        jump 125829137 ; Address of VRAM_Copy_Loop
        
    nop
    add r3 1 r3
    load 0 r1 ; SDRAM address counter
    load32 76800 r2 ; amount of pixels to write (320x240)
    jump 125829125 ; Address of SDRAM_Write_Loop
    nop
    nop
    nop
    halt

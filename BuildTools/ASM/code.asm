; Simple program to test some basic CPU operations

Main:
    nop
    load32 0x7B00000 r11
    load32 76800 r4
    add r11 r4 r11
    load 0 r2
    load32 0x7B00000 r1

    Loop:
        write 0 r1 r2
        add r1 1 r1
        add r2 1 r2

        beq r1 r11 2
        jump 125829129

    jump 125829128
        

Int:
    reti

Main:
    load 7 r1
    load32 0x1E400020 r2 ; VRAM32 word 8
    write 0 r2 r1
    read 0 r2 r3 ; r3=7

    load 3 r1
    write -12 r2 r1 ; VRAM32 word 5
    load32 0x1E400014 r6 ; VRAM32 word 5
    read 0 r6 r7 ; r7=3

    add r3 r7 r15 ; expected=10

    halt

Main:
    load 7 r1
    load32 0x7900008 r2 ; VRAM32 address 8
    write 0 r2 r1
    read 0 r2 r3 ; r3=7

    load 3 r1
    write -3 r2 r1
    load32 0x7900005 r6 ; VRAM32 address 5
    read 0 r6 r7 ; r7=3
    
    add r3 r7 r15 ; expected=10

    halt

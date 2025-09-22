; Bootloader mock to jump to SDRAM
Main:

    load 0 r1
    load 0 r2

    load 0 r3
    load 0 r4

    jump 0
    
    halt

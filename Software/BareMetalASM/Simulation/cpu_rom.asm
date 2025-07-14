Main:
    ; Note: if you want to use jumps, then only use offsets, unless you configure the assembler with the correct ROM base address
    
    ; Start with some dummy loads to mock bootloader behavior
    load 0 r1
    load 0 r2
    load 0 r3
    load 0 r4
    load 0 r5
    load 0 r6

    ; Jump to RAM code, which always starts at address 0
    jump 0

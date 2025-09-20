; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache line size of 8 words (32 bytes), meaning that address 8 is on the 2nd cache line
Main:
    load 7 r1 ; Dummy data
    load 8 r2 ; SDRAM address 8 (2nd cache line)
    write 0 r2 r1 ; Write 7 to 2nd cache line with offset 0
    
    ; Wait a bit
    nop
    nop
    nop

    ; Now read it back
    read 8 r0 r3 ; Read SDRAM address 8 into r3, should be 7
    
    ; Wait a bit
    nop
    nop
    nop
    
    add r3 0 r15 ; expected=7

    halt

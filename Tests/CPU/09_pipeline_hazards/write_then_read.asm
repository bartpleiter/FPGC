; Test write followed immediately by read from same address
; This pattern appears in B32CC generated code
; Note: Uses SDRAM address (stack at 0x77FFFF which becomes 0x77FFFE with -1 offset)
Main:
    load32 0x77FFFF r14   ; init base pointer to stack area
    
    load 42 r1            ; r1 = 42
    write -1 r14 r1       ; write 42 to address 0x77FFFE
    read -1 r14 r2        ; immediately read same address
    
    or r2 r0 r15          ; expected=42
    halt

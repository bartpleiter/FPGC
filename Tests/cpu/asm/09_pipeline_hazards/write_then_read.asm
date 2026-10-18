; Test write followed immediately by read from same address
; This pattern appears in B32CC generated code
; Note: Uses SDRAM address (stack at 0x77FFFC which becomes 0x77FFF8 with -4 offset)
Main:
    load32 0x77FFFC r14   ; init base pointer to stack area

    load 42 r1            ; r1 = 42
    write -4 r14 r1       ; write 42 to address 0x77FFF8
    read -4 r14 r2        ; immediately read same address

    or r2 r0 r15          ; expected=42
    halt

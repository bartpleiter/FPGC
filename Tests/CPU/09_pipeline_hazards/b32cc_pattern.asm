; Test exact pattern from B32CC generated code
; This mimics the store-to-load pattern from sub_shift_only.c
Main:
    ; Initialize stack/base pointer exactly like B32CC does
    load32 0x77FFFF r13   ; stack pointer
    load 0 r14            ; base pointer (initially 0)
    
    ; Setup frame like B32CC
    sub r13 6 r13         ; allocate stack frame
    write 4 r13 r14       ; save old r14
    add r13 4 r14         ; r14 = new base pointer
    
    ; Store values like fixedmath test
    load32 327680 r1      ; r1 = 5 << 16 = 0x50000
    write -1 r14 r1       ; store a
    load32 196608 r1      ; r1 = 3 << 16 = 0x30000  
    write -2 r14 r1       ; store b
    
    ; Read values back
    read -1 r14 r1        ; read a
    read -2 r14 r8        ; read b
    
    ; Subtract
    sub r1 r8 r1          ; c = a - b = 0x20000
    
    ; Store and immediately read (store-to-load hazard)
    write -3 r14 r1       ; store c
    read -3 r14 r4        ; read c into r4
    
    ; Verify r4 has correct value (0x20000 = 131072)
    ; Expected shift right by 16 = 2
    shiftrs r4 16 r15     ; expected=2
    halt

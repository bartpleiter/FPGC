; Test B32CC pattern with function call
; This mimics the exact store-to-load + function call pattern from sub_shift_only.c
Main:

    ; Skip this test is run from ROM:
    savpc r1
    load32 0x800000 r2
    blt r1 r2 3
    load 2 r15
    halt

    ; Initialize stack/base pointer exactly like B32CC does
    load32 0x77FFFF r13   ; stack pointer
    load 0 r14            ; base pointer (initially 0)
    
    ; Setup frame like B32CC
    sub r13 6 r13         ; allocate stack frame
    write 4 r13 r14       ; save old r14
    add r13 4 r14         ; r14 = new base pointer (0x77FFFD)
    
    ; Store values like fixedmath test
    load32 327680 r1      ; r1 = 5 << 16 = 0x50000
    write -1 r14 r1       ; store a at -1
    load32 196608 r1      ; r1 = 3 << 16 = 0x30000  
    write -2 r14 r1       ; store b at -2
    
    ; Read values back
    read -1 r14 r1        ; read a
    read -2 r14 r8        ; read b
    
    ; Subtract
    sub r1 r8 r1          ; c = a - b = 0x20000
    
    ; Store and immediately read (store-to-load hazard)
    write -3 r14 r1       ; store c at -3
    read -3 r14 r4        ; read c into r4 (argument register)
    
    ; Call function (exactly like B32CC does)
    sub r13 4 r13         ; adjust stack for call
    savpc r15             ; save return address
    add r15 3 r15         ; adjust for call
    jump shift_right_16   ; call function
    sub r13 -4 r13        ; restore stack after return
    
    ; r1 should now have the result (2)
    or r1 r0 r15          ; expected=2
    halt

; Function: shift_right_16(x) returns x >> 16
shift_right_16:
    write 0 r13 r4        ; save argument at r13
    sub r13 2 r13         ; allocate frame
    write 0 r13 r14       ; save old r14
    add r13 0 r14         ; r14 = new base pointer
    read 2 r14 r1         ; read argument (offset 2 from new r14)
    shiftrs r1 16 r1      ; shift right signed by 16
    read 0 r14 r14        ; restore old r14
    add r13 2 r13         ; restore stack
    jumpr 0 r15           ; return (result in r1)

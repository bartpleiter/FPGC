; Simplified C function call test
Main:

    ; Skip this test is run from ROM:
    savpc r1
    load32 0x800000 r2
    blt r1 r2 3
    load 42 r15
    halt

    load32 0x77FFFF r13   ; stack pointer
    
    ; Call function with argument in r4
    load 42 r4            ; argument
    sub r13 4 r13         ; adjust stack
    savpc r15
    add r15 3 r15
    jump identity_func
    sub r13 -4 r13        ; restore stack
    
    or r1 r0 r15          ; expected=42
    halt

identity_func:
    ; Simple function that returns its argument
    write 0 r13 r4        ; save arg at r13
    sub r13 2 r13         ; allocate frame
    write 0 r13 r14       ; save old r14
    add r13 0 r14         ; r14 = r13
    read 2 r14 r1         ; read arg from r14+2
    read 0 r14 r14        ; restore r14
    add r13 2 r13         ; restore r13
    jumpr 0 r15           ; return

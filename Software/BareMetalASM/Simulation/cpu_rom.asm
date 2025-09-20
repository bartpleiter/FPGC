; Tests specific edge case hazard that is triggered when two multicycle EXMEM2 instruction results are both used without arguments in between, e.g.
; - two memory read results are used as arguments without instructions in between, both reads being cache misses
; - two multiply results are used as arguments without instructions in between, both multiplies being multicycle
Main:

    ; Setup test data
    load 1 r1          ; Test data 1
    load 2 r2          ; Test data 2

    load 3 r3
    load 4 r4

    multu r3 r3 r10    ; r10 = 9
    multu r4 r4 r11    ; r11 = 16
    add r10 r11 r12    ; r12 = 25
    
    write 0 r0 r1
    write 1024 r0 r2
    
    read 0 r0 r5
    read 1024 r0 r6

    add r5 r6 r13       ; r13 = 3
    add r12 r13 r15     ; expected=28
    
    halt

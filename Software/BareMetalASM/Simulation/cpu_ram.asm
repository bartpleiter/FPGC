; Tests hazard where a flush because of a jump in EXMEM1, and long duration stall because of multicycle alu in EXMEM2 happen at the same time
; If not handled correctly, this will cause FE2 to handle the bubble as a valid instruction, executing the first instruction again
Main:

    load 37 r15
    load 43 r15

    multu r15 r15 r10     ; r10 = dont care
    jumpo 2
    nop

    or r0 r15 r15 ; expected=43
    halt

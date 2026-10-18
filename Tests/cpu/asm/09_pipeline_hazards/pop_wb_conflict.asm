; Test case: Pop hazard where pop result needed while also needing WB result
Main:
    load 10 r1          ; r1 = 10 -> will be in WB
    push r1             ; push 10
    load 5 r2           ; r2 = 5  -> will be in WB when add executes
    pop r3              ; r3 = 10, pop in EXMEM2, result available in WB
    add r2 r3 r15       ; EXMEM1 needs r2 (WB) AND r3 (from pop, not available until WB)
                        ; expected=15
    halt


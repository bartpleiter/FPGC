; Test immediate use of pop result (PC-1 dependency)
Main:
    load 7 r1           ; r1 = 7
    push r1             ; push 7
    pop r2              ; r2 = 7, pop in EXMEM2
    add r2 3 r15        ; immediately use r2 from pop (PC-1 dependency)
                        ; expected=10
    halt


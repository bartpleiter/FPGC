; Test push followed by read followed by pop (different register)
; This is a pattern seen in B32CC generated code
Main:
    load32 0x77FFFF r13  ; init stack pointer
    load 100 r5          ; r5 = 100
    push r5              ; push 100 onto stack
    load 42 r8           ; r8 = 42 (simulating a read)
    pop r11              ; r11 should be 100
    add r8 r11 r15       ; expected=142
    halt

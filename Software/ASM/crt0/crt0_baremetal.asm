; crt0_baremetal.asm — Startup code for bare metal programs
;
; Provides:
;   Main:  Stack/FP init → calls main() → sends return value over UART → halt
;   Int:   Saves all regs → calls interrupt() → restores regs → reti
;
; The C program must define:
;   int main()          — program entry point
;   void interrupt()    — interrupt handler (can be empty)
;
; Memory layout:
;   Stack: 0x1DFFFFC (top of SDRAM, grows down)
;   Int stack: same (bare metal has no OS stack separation)

.text

Main:
    load32 0 r14                ; initialize frame pointer
    load32 0x1DFFFFC r13        ; initialize stack pointer (top of SDRAM)
    savpc r15                   ; set return address
    add r15 12 r15              ; point r15 past the jump
    jump main                   ; call C main()
    ; main() returned — r1 holds return value
    load32 0x1C000000 r2        ; UART tx address
    write 0 r2 r1              ; send return value over UART
    halt                        ; done

Int:
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    load32 0x1DFFFFC r13        ; interrupt stack
    load32 0 r14                ; clear frame pointer
    savpc r15
    add r15 12 r15
    jump interrupt              ; call C interrupt()

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop r7
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1

    reti
    halt                        ; should not get here

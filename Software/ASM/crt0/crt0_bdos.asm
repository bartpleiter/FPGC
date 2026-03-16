; crt0_bdos.asm — Startup code for the BDOS kernel
;
; Provides:
;   Main:     Stack/FP init → calls main() → halt
;   Int:      Saves all regs → switches to interrupt stack → calls interrupt() → reti
;   Syscall:  Saves regs (except r1) → switches to syscall stack → calls bdos_syscall_dispatch() → return to user
;
; The C program must define:
;   int main()                      — kernel entry point
;   void interrupt()                — interrupt handler
;   int bdos_syscall_dispatch()     — syscall dispatcher (args in r4-r7, return in r1)
;
; Memory layout:
;   Main stack:      0x3DFFFC (BDOS main stack, grows down)
;   Interrupt stack: 0x3FFFFC (separate from main to avoid corruption)
;   Syscall stack:   0x3EFFFC (separate from main and interrupt)

.text

Main:
    load32 0 r14                ; initialize frame pointer
    load32 0x3DFFFC r13         ; initialize BDOS main stack
    savpc r15
    add r15 12 r15
    jump main                   ; call C main()
    ; main() returned
    load32 0x1C000000 r2
    write 0 r2 r1              ; send return value over UART
    halt

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

    load32 0x3FFFFC r13         ; BDOS interrupt stack
    load32 0 r14
    savpc r15
    add r15 12 r15
    jump interrupt              ; call C interrupt()

.global Return_Interrupt
Return_Interrupt:
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

Syscall:
    ; Save all registers except r1 (return value) to hardware stack
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

    load32 0x3EFFFC r13         ; syscall stack
    sub r13 16 r13              ; allocate arg save area for C callee
    load32 0 r14
    ; Use r11 (temp reg) to avoid clobbering r4-r7 (syscall args)
    addr2reg Return_Syscall r11
    or r0 r11 r15
    jump bdos_syscall_dispatch  ; call C dispatcher (args in r4=num, r5=a1, r6=a2, r7=a3)
    halt                        ; should not get here

Return_Syscall:
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

    jumpr 0 r15                 ; return to user program
    halt                        ; should not get here

; syscall_asm.asm — Low-level syscall invocation for userBDOS programs
;
; Provides: int syscall(int num, int a1, int a2, int a3)
;
; ABI: r4=num, r5=a1, r6=a2, r7=a3 (standard calling convention)
; The BDOS syscall handler is at byte address 12 (set by ASMPY -s flag).
; Result is returned in r1.
;
; NOTE: Uses the software stack (r13) instead of the hardware stack
; to save registers.  The HW stack is reserved for the kernel's
; context_enter/return mechanism.  Any user HW stack entries would
; interfere with blocking syscall support (Phase 3+).

.text

syscall:
    ; r4=num, r5=a1, r6=a2, r7=a3 already in place from caller
    ; r15 = return address to our caller
    ; Save r15 and r11 to software stack (not HW stack!)
    sub r13 8 r13               ; allocate 2 words on software stack
    write 4 r13 r15             ; save caller's return address
    write 0 r13 r11             ; save callee-saved r11

    load 12 r11                 ; r11 = 12 (syscall vector byte address)
    savpc r15                   ; r15 = PC of this instruction
    add r15 12 r15              ; r15 = return point (3 instructions forward)
    jumpr 0 r11                 ; jump to BDOS syscall handler
    ; Syscall handler returns here with result in r1
    read 0 r13 r11              ; restore r11
    read 4 r13 r15              ; restore caller's return address
    add r13 8 r13               ; deallocate
    jumpr 0 r15                 ; return to caller

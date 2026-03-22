; syscall_asm.asm — Low-level syscall invocation for userBDOS programs
;
; Provides: int syscall(int num, int a1, int a2, int a3)
;
; ABI: r4=num, r5=a1, r6=a2, r7=a3 (standard calling convention)
; The BDOS syscall handler is at byte address 12 (set by ASMPY -s flag).
; Result is returned in r1.

.text

syscall:
    ; r4=num, r5=a1, r6=a2, r7=a3 already in place from caller
    ; r15 = return address to our caller (set by savpc+add+jump in the caller's code)
    ; We need to:
    ;   1. Save our return address (r15)
    ;   2. Set r15 to point past the jumpr so the syscall handler returns here
    ;   3. Jump to the syscall vector at byte address 12
    ;   4. Restore our return address and return

    push r15                    ; save caller's return address to hardware stack
    push r11                    ; save callee-saved r11
    load 12 r11                 ; r11 = 12 (syscall vector byte address)
    savpc r15                   ; r15 = PC of this instruction
    add r15 12 r15              ; r15 = return point (3 instructions forward = 12 bytes)
    jumpr 0 r11                 ; jump to BDOS syscall handler
    ; Syscall handler returns here with result in r1
    pop r11                     ; restore r11
    pop r15                     ; restore caller's return address
    jumpr 0 r15                 ; return to caller

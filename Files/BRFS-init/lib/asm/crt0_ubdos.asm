; crt0_ubdos.asm — Startup code for userBDOS programs (runs under BDOS)
;
; Provides:
;   Main:  calls main() → SYS_EXIT with return value
;   Int:   reti stub (BDOS handles all interrupts)
;
; The C program must define:
;   int main()  — program entry point
;
; Note: The kernel's context_enter loads all registers from the process
; table before jumping here.  SP (r13) and FP (r14) are already set.
; On return from main(), we call SYS_EXIT to cleanly terminate.

.text

Main:
    load32 0 r14                ; initialize frame pointer
    savpc r15
    add r15 12 r15
    jump main                   ; call C main()
    ; main() returned — r1 holds return value
    or r0 r1 r5                 ; r5 = exit code (syscall arg1)
    load 1 r4                   ; r4 = SYS_EXIT (1)
    load 12 r11                 ; r11 = syscall vector address
    savpc r15
    add r15 12 r15
    jumpr 0 r11                 ; invoke SYS_EXIT — does not return
    halt                        ; should not get here

; User programs don't handle interrupts — BDOS handles them.
; This stub is required because the ASMPY -h header emits 'jump Int'.
Int:
    reti
    halt                        ; should not get here

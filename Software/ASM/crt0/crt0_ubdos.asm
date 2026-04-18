; crt0_ubdos.asm — Startup code for userBDOS programs (runs under BDOS)
;
; Provides:
;   Main:  FP init → calls main() → returns to BDOS via r15
;   Int:   reti stub (BDOS handles all interrupts)
;
; The C program must define:
;   int main()  — program entry point
;
; Note: The BDOS loader sets r13 (stack pointer) and r15 (return address)
; before jumping to Main. We only initialize r14 (frame pointer).
; We save the BDOS return address and restore it after main() returns.

.text

Main:
    load32 0 r14                ; initialize frame pointer (SP and return addr set by BDOS)
    push r15                    ; save BDOS return address to hardware stack
    savpc r15
    add r15 12 r15
    jump main                   ; call C main()
    ; main() returned — r1 holds return value
    pop r15                     ; restore BDOS return address
    jumpr 0 r15                 ; return to BDOS
    halt                        ; should not get here

; User programs don't handle interrupts — BDOS handles them.
; This stub is required because the ASMPY -h header emits 'jump Int'.
Int:
    reti
    halt                        ; should not get here

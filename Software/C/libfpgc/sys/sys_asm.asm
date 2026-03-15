; sys_asm.asm — System-level assembly functions for B32P3
;
; Provides functions that need special CPU instructions not available in C:
;   get_int_id()   — readintid instruction
;
; B32P3 calling convention: return value in r1

.global get_int_id

; int get_int_id(void)
; Read the current interrupt identifier from the CPU.
get_int_id:
    readintid r1
    jumpr 0 r15

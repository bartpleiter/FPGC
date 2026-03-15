; _exit.asm — Program termination for B32P3/FPGC
;
; Provides _exit(int code):
;   1. Writes exit code to UART TX (so host can read it)
;   2. Halts the CPU
;
; Called from libc exit()/abort()/__assert_fail().
; Argument: r4 = exit code (B32P3 calling convention: first arg in r4)

.global _exit

_exit:
    load32 0x1C000000 r2        ; UART TX address
    write 0 r2 r4              ; send exit code over UART
    halt                        ; stop the CPU

; hwio.asm — Hardware I/O functions for B32P3/FPGC
;
; Provides memory-mapped I/O write/read functions that cproc can't emit
; (cproc doesn't support volatile stores).
;
; B32P3 calling convention: args in r4, r5, r6, r7; return in r1

; void hwio_write(int addr, int value)
;   Write value to memory-mapped I/O register at addr.
;   r4 = address, r5 = value
.global hwio_write
hwio_write:
    write 0 r4 r5
    jumpr 0 r15

; int hwio_read(int addr)
;   Read value from memory-mapped I/O register at addr.
;   r4 = address; returns value in r1
.global hwio_read
hwio_read:
    read 0 r4 r1
    jumpr 0 r15

; The demo of MEM (fpgc cpu wave cpu_mem).
; A write writes the data memory at the end of MEM. A read gets its word from the data
; memory in MEM, and takes the bytes it wants out of it. A push and a pop use the stack,
; and an access at 0x1F000000 the CPU-internal I/O registers. The error register
; records an illegal access when it leaves MEM. The redirects come from here too.
load 4096 r1
load 0xBEEF r2
write 0 r1 r2       ; writes 0x0000BEEF
read 0 r1 r3        ; waits a cycle in EX for the write, then reads 0x0000BEEF
readb 0 r1 r4       ; the byte 0xEF, sign-extended: 0xFFFFFFEF
readbu 1 r1 r5      ; the byte 0xBE, zero-extended
add r5 1 r6         ; waits a cycle in ID for the read
writeh 2 r1 r6      ; r6 from MEM, written as the upper half: 0x00BF0000
read 0 r1 r7        ; waits a cycle in EX for the write, then reads 0x00BFBEEF
push r7             ; waits a cycle in ID, r7 from WB: pushed at SP 0
pop r8              ; waits a cycle in EX for the push, then pops 0x00BFBEEF
load32 0x1F000000 r9
readh 4 r9 r10      ; a halfword of SP: a bad address, so it reads 0 and sets a flag
read 8 r9 r11       ; ERR_STATUS: 0x304
bne r11 r0 End      ; waits a cycle in ID, r11 from WB: taken, so it flushes three
add r0 1 r12
add r0 2 r12
add r0 3 r12
End:
halt

; The demo of MEM (fpgc cpu wave cpu_mem).
; A write writes the data memory at the end of MEM. A read gets its word from the data
; memory in MEM, and takes the bytes it wants out of it. The redirects come from here.
load 4096 r1
load 0xBEEF r2
write 0 r1 r2       ; writes 0x0000BEEF
read 0 r1 r3        ; waits a cycle in EX for the write, then reads 0x0000BEEF
readb 0 r1 r4       ; the byte 0xEF, sign-extended: 0xFFFFFFEF
readbu 1 r1 r5      ; the byte 0xBE, zero-extended
add r5 1 r6         ; waits a cycle in ID for the read
writeh 2 r1 r6      ; r6 from MEM, written as the upper half: 0x00BF0000
read 0 r1 r7        ; waits a cycle in EX for the write, then reads 0x00BFBEEF
bne r7 r0 End       ; r7 from WB: taken, so it flushes the three instructions behind it
add r0 1 r8
add r0 2 r9
add r0 3 r10
End:
halt

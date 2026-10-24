; The demo of ID (fpgc cpu wave cpu_id).
; Decoding, reading the registers, and deciding where each value will come from in EX:
; forwarded from MEM or WB, or read here. The register write in WB goes through to the
; read in the same cycle. The target adder adds the constant to the PC. The late-result
; stall comes with the first late results, the loads.
load 5 r1
add r0 -3 r2        ; the constant is sign-extended
add r1 r2 r3        ; r2 from MEM, r1 from WB
sub r3 r1 r4        ; r3 from MEM, r1 from the register file, written in this cycle
jumpo 8             ; the target is the PC + 8
add r0 1 r5         ; flushed
savpc r6            ; the constant is the PC, from the target adder too
halt                ; the target is the PC + 0

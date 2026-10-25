; The demo of EX (fpgc cpu wave cpu_ex).
; The ALU, with a register or the constant as its second operand, and the forwarding
; mux in front of it. The address adder, which gives the address of a read or write. A
; read or write waits while a write is in MEM, and keeps the values forwarded to it.
; Then the branch compare, and the address adder again, which gives jumpr its target.
load 12 r1
load 10 r2
and r1 r2 r3        ; r2 from MEM, r1 from WB
or r3 5 r4          ; r3 from MEM
shiftl r3 2 r5      ; r3 from WB
slt r4 r1 r6        ; r4 from WB, r1 from the register file
load 4096 r9
add r9 4 r10        ; r9 from MEM
write 0 r9 r10      ; r10 from MEM, r9 from WB: writes 4100 at 4096
read -4 r10 r11     ; r10 from WB, kept while it waits a cycle for the write
beq r6 r0 Next      ; r6 from MEM, so 0 = 0: taken
add r0 1 r7         ; flushed
Next:
savpc r8
jumpr 12 r8         ; r8 from MEM, plus 12: the halt
add r0 2 r7         ; flushed
halt

; The overview demo of the whole pipeline (fpgc cpu wave b32p4).
; One instruction enters the pipeline every cycle. An instruction that reads a register
; an instruction ahead of it still has to write gets the value forwarded. A taken
; branch redirects from MEM, and flushes the three instructions behind it. HALT
; redirects to itself.
load 1 r1
load 2 r2
add r1 r2 r3        ; r2 from MEM, r1 from WB
xor r3 r1 r4        ; r3 from MEM
bne r4 r0 End       ; r4 from MEM, taken
add r0 1 r5         ; flushed
add r0 2 r6         ; flushed
add r0 3 r7         ; flushed
End:
halt

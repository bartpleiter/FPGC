; The demo of MEM (fpgc cpu wave cpu_mem).
; For now the ALU result passes through. The redirects come from here.
load 1 r1
add r1 r1 r2        ; r1 forwarded from here
bne r2 r0 End       ; taken, so it flushes the three instructions behind it
add r0 1 r3
add r0 2 r4
add r0 3 r5
End:
halt

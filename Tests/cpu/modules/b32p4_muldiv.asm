; Multiply and divide in the pipeline (fpgc cpu wave b32p4_muldiv).
; A multiply starts in EX and has its result at the end of MEM, so a new one can start
; every cycle, and an instruction that needs the result right away waits a cycle in ID,
; as after a read. A divide waits in EX until the divider is done.
load 100 r1
load 7 r2
mults r1 r2 r3      ; 700
mults r1 r1 r4      ; 10000, right behind the first multiply
add r3 r4 r5        ; 10700: waits a cycle in ID for r4
divu r5 r2 r6       ; 1528: waits in EX until the divider is done
add r6 r2 r7        ; 1535: waits in ID behind the divide, and a cycle more for r6
halt

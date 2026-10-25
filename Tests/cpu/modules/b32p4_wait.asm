; The pipeline with a data memory that waits (fpgc cpu wave b32p4_wait --data-latency 3).
; A read or write makes its request when it enters MEM, and waits there until the data
; memory answers. The instructions behind it wait too, and WB gets bubbles. The comments
; are for a data memory that waits 3 cycles.
load 4096 r1
load 7 r2
write 0 r1 r2       ; waits 3 cycles in MEM, then writes 7
read 0 r1 r3        ; waits a cycle in EX for the write, then 3 cycles in MEM: 7
add r3 r2 r4        ; waits in ID for the read, then r3 from WB
halt

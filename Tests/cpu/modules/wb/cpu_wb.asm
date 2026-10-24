; The demo of WB (fpgc cpu wave cpu_wb).
; The register writes, and the retire port. Instructions that write r0 don't write.
load 1 r1
load 2 r2
add r1 5 r0         ; r1 forwarded from here
halt

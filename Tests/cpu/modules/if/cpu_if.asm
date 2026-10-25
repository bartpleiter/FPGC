; The demo of IF (fpgc cpu wave cpu_if).
; The PC moves on every cycle, stands still while ID stalls, and follows the redirect.
load 7 r1
read 0 r0 r2
add r2 r1 r3        ; waits a cycle in ID for the read, so the PC stands still
jumpo 12            ; the PC continues at the halt
nop                 ; fetched, then flushed
nop
halt

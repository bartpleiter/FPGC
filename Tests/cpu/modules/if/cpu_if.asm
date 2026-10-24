; The demo of IF (fpgc cpu wave cpu_if).
; The PC moves on every cycle and follows the redirect. It stands still while ID
; stalls, which comes back with the late-result stall of the loads.
load 7 r1
add r1 1 r2
jumpo 12            ; the PC continues at the halt
nop                 ; fetched, then flushed
nop
halt

; The demo of IF (fpgc cpu wave cpu_if).
; It runs from SDRAM, so IF fetches through the L1I. After the jump from the ROM, IF
; waits for the cache controller's walk after reset. The first fetch misses: IF requests
; the line, and waits while the cache controller fills it. The words arrive from the one
; IF waits for, and IF passes each one on in the cycle it arrives (early restart), so
; the PC moves on every cycle. While ID stalls, the PC stands still, and the word that
; arrives then is lost: IF waits for the fill to end, then reads the L1I again and hits.
; After that the PC moves on every cycle, and follows the redirect.
; runs from SDRAM
load 7 r1
mults r1 r1 r2
add r2 r1 r3        ; waits a cycle in ID for the multiply, so the PC stands still
jumpo 8             ; the PC continues at the halt
nop                 ; fetched, then flushed
halt

; dma_asm.asm — DMA-related assembly helpers for B32P3
;
; Provides the cache_flush_data() function which executes the `ccached`
; instruction (data-cache-only flush + invalidate). cproc cannot emit
; ccached directly, so it lives here.
;
; B32P3 calling convention: return via `jumpr 0 r15`.

.global cache_flush_data

; void cache_flush_data(void)
cache_flush_data:
    ccached
    jumpr 0 r15

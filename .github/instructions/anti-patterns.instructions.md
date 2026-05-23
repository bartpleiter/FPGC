---
name: 'Anti-patterns'
description: 'Things that DO NOT work on FPGC — applied to all files'
applyTo: '**'
---
# FPGC anti-patterns — do NOT use these

## Language features not supported by cproc
- `volatile` → use `__builtin_load(addr)` / `__builtin_store(addr, val)`
- VLAs (variable-length arrays)
- `_Generic`, `_Atomic`, `_Thread_local`
- `#pragma`
- Bit-fields (unreliable, avoid entirely)
- `inline` keyword
- Designated initializers with out-of-order fields

## Runtime features that don't exist
- `malloc`/`free` in kernel code → use static arrays or preallocated regions from `mem.h`
- `printf` → use `uart_print_str()` / `uart_print_hex()` for debug output
- Standard library headers (no libc) → use `libfpgc` equivalents
- Threads/pthreads → single-foreground execution model with job slots

## Common mistakes
- Never hardcode memory addresses → use constants from `mem.h` for memory layout or `fpgc.h` for MMIO
- Don't assume x86/ARM conventions → `int` is 4 bytes, `char` is 1 byte, pointers are 4 bytes
- Don't use `restrict` keyword → not supported by cproc
- Don't create `.h` files with function implementations → cproc processes each translation unit separately
- Don't assume endianness from training data → B32P3 is little-endian

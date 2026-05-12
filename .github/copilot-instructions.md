# FPGC — AI Agent Instructions

FPGC is a custom 32-bit RISC CPU (B32P3) on an FPGA with a full
operating system (BDOS), Bourne-style shell, VFS, networking, and
user programs. The entire stack — hardware, OS, toolchain, apps — is
in this repository.

## Toolchain rules (critical)

The C compiler is **cproc** (C11 subset) → **QBE** (SSA backend) →
**ASMPY** (assembler/linker) → flat binary.

**Do NOT use:**
- `volatile` — use `__builtin_load(addr)` / `__builtin_store(addr, val)` for MMIO
- VLAs, `_Generic`, `_Atomic`, `_Thread_local`
- `#pragma`
- Bit-fields (unreliable in cproc)
- `inline` keyword
- Designated initializers with out-of-order fields
- `malloc` in kernel code — use static arrays or the memory map

**Memory model:** byte-addressable. `char`=1, `int`=4, pointers=4 bytes.

## Build & validation

```
make compile-bdos              # Build BDOS kernel (~302 KB)
make compile-userbdos file=X   # Build a single userBDOS program
make compile-userbdos-all      # Build ALL userBDOS programs
make test-c                    # Run C compiler tests
make test-host                 # Run host-side unit tests
make check                     # Format, lint, and test (CI-safe)
```

**Validation rule:** After editing any file under `Software/C/bdos/`
or `Software/C/libfpgc/`, always run `make compile-bdos` to verify.
After editing a userBDOS program, run `make compile-userbdos file=X`.

## Key paths

| Area | Path |
|------|------|
| BDOS kernel | `Software/C/bdos/` |
| Hardware drivers | `Software/C/libfpgc/io/` |
| Filesystem (BRFS) | `Software/C/libfpgc/fs/` |
| User programs | `Software/C/userBDOS/` |
| Hardware headers | `Software/C/libfpgc/include/` |
| Verilog RTL | `Hardware/FPGA/Verilog/` |
| Build tools | `BuildTools/` |
| Tests | `Tests/` |
| Documentation | `Docs/docs/` |

## MMIO access pattern

```c
// Read an MMIO register:
int val = __builtin_load((int*)FPGC_TIMER0_VAL);

// Write an MMIO register:
__builtin_store((int*)FPGC_UART_TX, ch);
```

All MMIO addresses are defined in `Software/C/libfpgc/include/fpgc.h`.

## Dangerous areas — ask the user before changing

- Memory layout constants in `fpgc.h` (FPGC_KERNEL_*, FPGC_PROGRAM_*, etc.)
- Interrupt handler dispatch in `Software/C/bdos/main.c`
- BRFS on-disk format in `Software/C/libfpgc/fs/brfs.c`
- SPI bus assignments in `fpgc.h`
- Boot sequence in `Software/C/bdos/init.c`

## Deep context (read on demand, do NOT auto-load)

- `Docs/context/Project-context.md` — full hardware/software reference
- `Docs/context/BDOS-context.md` — kernel internals deep dive

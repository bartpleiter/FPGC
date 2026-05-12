---
name: 'Toolchain'
description: 'Rules for editing the cproc/QBE/ASMPY build toolchain'
applyTo: 'BuildTools/**'
---
# Toolchain guidelines

## Architecture
```
C source → cproc (C11 frontend) → QBE IR → QBE (SSA backend) → B32P3 ASM → ASMPY (assembler/linker) → flat binary
```

## Build
```
make cproc              # Build cproc C frontend
make qbe                # Build QBE backend
make asmpy-install      # Install ASMPY assembler
make test-c             # Run all C compiler tests (parallel)
make test-c-single file=<test>  # Run a single C test
make test-asmpy         # Run ASMPY-specific tests
make test-asm-link      # Run asm-link byte-for-byte regression tests
make test-cpp           # Run cpp byte-for-byte regression tests vs gcc cpp
```

## Key paths
| Component | Source | Output |
|-----------|--------|--------|
| cproc | `BuildTools/cproc/` | `BuildTools/cproc/output/cproc-qbe` |
| QBE | `BuildTools/QBE/` | `BuildTools/QBE/output/qbe` |
| ASMPY | `BuildTools/ASMPY/` | Python package (pip install) |
| B32P3 target | `BuildTools/QBE/b32p3/` | QBE backend for B32P3 ISA |

## cproc limitations
cproc implements a C11 subset. Do NOT use:
- `volatile`, `restrict`
- VLAs, `_Generic`, `_Atomic`, `_Thread_local`
- `#pragma`
- Bit-fields
- `inline` keyword
- Complex designated initializers

## Common build errors and what they mean
- "unknown type qualifier" → you used `volatile` or `restrict`
- "invalid operand" in QBE → likely an unsupported C feature compiled through
- "label X not found" in ASMPY → missing function, check linking order
- "file too large" → binary >512 KB won't fit in SPI flash
- "undefined symbol" → function not included in the link list or misspelled

## Self-hosting
The toolchain can be compiled to run ON the FPGC itself:
```
make selfhost-qbe       # Build QBE as a userBDOS binary
make selfhost-cproc     # Build cproc as a userBDOS binary
make selfhost-all       # Build both
```

## Ripple effects
- Changing cproc code generation → run `make test-c` (full suite)
- Changing QBE B32P3 backend → run `make test-c` AND `make compile-bdos`
- Changing ASMPY → run `make test-asmpy` AND `make test-asm-link`

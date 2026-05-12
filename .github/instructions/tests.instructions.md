---
name: 'Tests'
description: 'Rules for editing and adding tests'
applyTo: 'Tests/**'
---
# Test guidelines

## Test categories and commands
| Target | What it tests | Path |
|--------|--------------|------|
| `make test-c` | C compiler (cproc→QBE→ASMPY pipeline) | `Tests/C/` |
| `make test-cpu` | CPU Verilog simulation | `Tests/CPU/` |
| `make test-asmpy` | ASMPY assembler | `BuildTools/ASMPY/tests/` |
| `make test-asm-link` | Assembler/linker regression | `Tests/asm-link/` |
| `make test-cpp` | C preprocessor vs gcc | (preprocessor tests) |
| `make test-host` | All host-side C unit tests | `Tests/host/` |
| `make test-shell-host` | BDOS shell host tests | `Tests/host/` |
| `make test-term` | libterm host tests | `Tests/host/` |
| `make test-vfs-pixpal` | /dev/pixpal VFS tests | `Tests/host/` |
| `make check` | Format + lint + all tests (CI) | — |

## Single test execution
```
make test-c-single file=<test_file>       # Single C compiler test
make test-cpu-single file=<test_file>     # Single CPU simulation test
```

## Adding a new test
1. Create the test file in the appropriate `Tests/` subdirectory
2. Follow the naming convention of existing tests in that directory
3. Run the full test suite to verify: `make test-c` or `make test-cpu`

## Host tests
Host tests run on the development machine (not on FPGC). They test
logic that can be compiled with gcc/clang. Located in `Tests/host/`.

## CPU simulation tests
These use Icarus Verilog to simulate the B32P3 CPU running assembly
programs. Debug with: `make debug-cpu file=<test_file>` (opens GTKWave).

# Development Workflows

This guide covers the development workflows for the main components of the FPGC project. Each section describes how to make changes, test them, and debug issues.

!!! note
    All commands should be run from the project root directory.

## Quick Reference

| Component | Test Command | Debug Command |
|-----------|--------------|---------------|
| Verilog (CPU) | `make test-cpu` | `make sim-cpu` |
| Verilog (GPU) | - | `make sim-gpu` |
| Verilog (SDRAM) | - | `make sim-sdram` |
| ASMPY Assembler | `make asmpy-test` | - |
| B32CC Compiler | `make test-b32cc` | `make debug-b32cc file=<test>` |

---

## Verilog

The Verilog sources live in `Hardware/FPGA/Verilog/`. Write or modify modules, then verify via simulation.

**CPU tests:**

```bash
make test-cpu
```

Runs all assembly tests in `Tests/CPU/`, checking UART output. Each test executes twice: once from ROM and once from RAM/SDRAM (to exercise cache and memory controller paths).

**Interactive CPU simulation:**

```bash
make sim-cpu
```

Builds `Software/BareMetalASM/Simulation/sim_rom.asm` (ROM), `sim_ram.asm` (RAM), and `sim_spiflash1.asm` (SPI Flash 1), runs the simulation, and opens GTKWave with preconfigured views. To simulate running a program via the UART bootloader (`sim_uartprog.asm`), use `make sim-cpu-uart`.

**GPU-only simulation:**

```bash
make sim-gpu
```

Runs the GPU testbench, opens GTKWave, and outputs PPM images of each rendered frame for visual verification.

**SDRAM controller:**

```bash
make sim-sdram
```

Runs the SDRAM controller testbench to verify timing and functionality (primarily used during initial development).

**Add a CPU test:**

```asm
; Tests/CPU/my_new_test.asm
Main:
   load 37 r15 ; expected=37
   halt
```

Expectations: entry label `Main`, result in `r15`, and an `; expected=XX` comment. Run with `make test-cpu`.

---

## ASMPY Assembler

ASMPY (Python-based) is in `BuildTools/ASMPY/`.

**Run tests:**

```bash
make asmpy-test
```

**Lint and checks:**

```bash
make lint
```

After changes, it’s good to run assembler-dependent tests to see if those still pass:

```bash
make test-cpu
make test-b32cc
```

---

## B32CC Compiler

B32CC (based on SmallerC) resides in `BuildTools/B32CC/`.

**Build:**

```bash
make b32cc
```

The compiler rebuilds automatically when `smlrc.c` or `cgb32p2.inc` changes.

**Run all tests:**

```bash
make test-b32cc
```

Compiles all C tests in `Tests/C/`, simulates, and verifies results.

**Single test:**

```bash
make test-b32cc-single file=3_1_if_statements.c
```

Faster when iterating on one feature.

**Debug a test:**

```bash
make debug-b32cc file=3_1_if_statements.c
```

Workflow: compile C to assembly, copy to `Software/BareMetalASM/Simulation/sim_ram.asm`, set ROM to jump to RAM, then open simulation via `make sim-cpu`.

**Add a C test:**

```c
// Tests/C/my_test.c
int main() {
    int result = 42;
    return result; // expected=0x2A
}

void interrupt() {
    // Required interrupt handler
}
```

Include `// expected=0xXX` for the return value. Run with:

```bash
make test-b32cc-single file=my_test.c
```

**Test categories:**

- `1_*`: basics (returns, variables)
- `2_*`: functions (calls, args, returns)
- `3_*`: control flow (if/while/for, combined)
- `4_*`: ...

---

## Documentation

The docs use MkDocs with Material.

**Serve locally:**

```bash
make docs-serve
```

Opens `http://localhost:8088` with live reload.

**Deploy:**

```bash
make docs-deploy
```

Deploys to my personal server (requires SSH access).

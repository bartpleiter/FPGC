---
name: 'Assembly'
description: 'Rules for editing B32P3 assembly files'
applyTo: 'Software/ASM/**'
---
# B32P3 assembly guidelines

## Build
```
make compile-asm file=<filename>       # Compile ASM file
make run-asm-uart                      # Compile and run via UART
```

## ISA quick reference
B32P3 is a 32-bit RISC ISA. Full reference: `Docs/docs/Hardware/CPU/CPU.md`

**Registers:** r0–r15, where:
- r0 = zero register (always 0)
- r13 = stack pointer (SP)
- r14 = frame pointer (FP)
- r15 = return address (RA)

**Key instructions:**
- `LOAD addr reg` / `STORE reg addr` — memory access
- `ADD`, `SUB`, `MULT`, `AND`, `OR`, `XOR`, `SHIFTL`, `SHIFTR`
- `BEQ`, `BNE`, `BGT`, `BGE`, `BLT`, `BLE` — conditional branch
- `JUMP addr` / `JUMPR reg` — unconditional jump
- `SAVPC reg` — save PC+1 to register (for call: `SAVPC r15; JUMP func`)
- `PUSH reg` / `POP reg` — stack operations
- `HALT` — stop CPU

**Pseudo-instructions (ASMPY):**
- `.dw value` — define word
- `.dd value` — define double word
- `.ds "string"` — define string
- `.dl label` — define label address

## Label conventions
- Function labels: `FunctionName:`
- Local labels: `.localLabel:`
- Constants: `CONSTANT_NAME`

## Ripple effects
- Changing ISA encoding → update `BuildTools/ASMPY/`, CPU Verilog, QBE B32P3 backend
- Changing calling convention → update QBE backend AND all handwritten ASM

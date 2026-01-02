# ISA

The B32P3 instruction set architecture is a 32-bit RISC architecture designed for the FPGC.

## Instruction Encoding

Each instruction is 32 bits and follows one of the formats below:

```text
         |31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
----------------------------------------------------------------------------------------------------------
1 HALT     1  1  1  1| 1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1 
2 READ     1  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---| x  x  x  x |--D REG---|
3 WRITE    1  1  0  1||----------------16 BIT CONSTANT---------------||--A REG---||--B REG---| x  x  x  x 
4 INTID    1  1  0  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
5 PUSH     1  0  1  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--B REG---| x  x  x  x 
6 POP      1  0  1  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
7 JUMP     1  0  0  1||--------------------------------27 BIT CONSTANT--------------------------------||O|
8 JUMPR    1  0  0  0||----------------16 BIT CONSTANT---------------| x  x  x  x |--B REG---| x  x  x |O|
9 CCACHE   0  1  1  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x 
10 BRANCH  0  1  1  0||----------------16 BIT CONSTANT---------------||--A REG---||--B REG---||-OPCODE||S|
11 SAVPC   0  1  0  1| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x |--D REG---|
12 RETI    0  1  0  0| x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x 
13 ARITHMC 0  0  1  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
14 ARITHM  0  0  1  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
15 ARITHC  0  0  0  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
16 ARITH   0  0  0  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
```

## Instruction Reference

### 1. HALT

Halts the CPU by continuously jumping to the same address. Can be interrupted.

**Encoding**: `0xFFFFFFFF`

**Operation**: `PC ← PC` (infinite loop)

---

### 2. READ

Read from memory at address `AREG + offset`, store value in `DREG`.

**Encoding**: `1110 | const16 | AREG | xxxx | DREG`

**Operation**: `DREG ← MEM[AREG + sign_extend(const16)]`

---

### 3. WRITE

Write value from `BREG` to memory at address `AREG + offset`.

**Encoding**: `1101 | const16 | AREG | BREG | xxxx`

**Operation**: `MEM[AREG + sign_extend(const16)] ← BREG`

---

### 4. INTID

Store the current interrupt ID in `DREG`. Returns 1-8 for interrupt sources.

**Encoding**: `1100 | xxxx...xxxx | DREG`

**Operation**: `DREG ← interrupt_id`

---

### 5. PUSH

Push value from `BREG` onto the hardware stack.

**Encoding**: `1011 | xxxx...xxxx | BREG | xxxx`

**Operation**: `STACK[SP] ← BREG; SP ← SP + 1`

---

### 6. POP

Pop value from hardware stack into `DREG`.

**Encoding**: `1010 | xxxx...xxxx | DREG`

**Operation**: `SP ← SP - 1; DREG ← STACK[SP]`

---

### 7. JUMP

Unconditional jump to address specified by 27-bit constant.

**Encoding**: `1001 | const27 | O`

**Operation**:

- If `O = 0`: `PC ← {5'b0, const27}` (absolute jump)
- If `O = 1`: `PC ← PC + {5'b0, const27}` (relative jump)

---

### 8. JUMPR

Jump to address computed from register plus offset.

**Encoding**: `1000 | const16 | xxxx | BREG | xxx | O`

**Operation**:

- If `O = 0`: `PC ← BREG + sign_extend(const16)` (absolute)
- If `O = 1`: `PC ← PC + BREG + sign_extend(const16)` (relative)

---

### 9. CCACHE

Clear all L1 cache by invalidating all cache lines and writing dirty lines back to SDRAM.

**Encoding**: `0111 | xxxx...xxxx`

**Operation**: Sets all valid bits in L1I and L1D cache to 0, writes back dirty lines to SDRAM.

---

### 10. BRANCH

Conditional branch based on comparison of `AREG` and `BREG`.

**Encoding**: `0110 | const16 | AREG | BREG | branchOP | S`

**Operation**: If condition is true: `PC ← PC + sign_extend(const16)`

---

### 11. SAVPC

Save current program counter to `DREG`.

**Encoding**: `0101 | xxxx...xxxx | DREG`

**Operation**: `DREG ← PC`

---

### 12. RETI

Return from interrupt. Restores PC and re-enables interrupts.

**Encoding**: `0100 | xxxx...xxxx`

**Operation**: `PC ← PC_backup; int_disabled ← 0`

---

### 13. ARITHMC

Multi-cycle arithmetic operation with immediate constant.

**Encoding**: `0011 | aluOP | const16 | AREG | DREG`

**Operation**: `DREG ← AREG op sign_extend(const16)` (multi-cycle)

---

### 14. ARITHM

Multi-cycle arithmetic operation with register operands.

**Encoding**: `0010 | aluOP | xxxx...xxxx | AREG | BREG | DREG`

**Operation**: `DREG ← AREG op BREG` (multi-cycle)

---

### 15. ARITHC

Single-cycle ALU operation with immediate constant.

**Encoding**: `0001 | aluOP | const16 | AREG | DREG`

**Operation**: `DREG ← AREG op sign_extend(const16)` (single-cycle)

---

### 16. ARITH

Single-cycle ALU operation with register operands.

**Encoding**: `0000 | aluOP | xxxx...xxxx | AREG | BREG | DREG`

**Operation**: `DREG ← AREG op BREG` (single-cycle)

---

## Branch Opcodes

The branch instruction supports 6 comparison operations:

| Operation | Opcode | Description | Signed Version |
|-----------|--------|-------------|----------------|
| BEQ       | `000`  | Branch if A == B | Same |
| BGT       | `001`  | Branch if A > B | BGTS |
| BGE       | `010`  | Branch if A >= B | BGES |
| (reserved)| `011`  | Reserved | - |
| BNE       | `100`  | Branch if A != B | Same |
| BLT       | `101`  | Branch if A < B | BLTS |
| BLE       | `110`  | Branch if A <= B | BLES |
| (reserved)| `111`  | Reserved | - |

Setting the `S` (sign) bit enables signed comparison for BGT, BGE, BLT, and BLE.

---

## ARITH Opcodes (Single-Cycle)

| Operation | Opcode | Description |
|-----------|--------|-------------|
| OR        | `0000` | A OR B |
| AND       | `0001` | A AND B |
| XOR       | `0010` | A XOR B |
| ADD       | `0011` | A + B |
| SUB       | `0100` | A - B |
| SHIFTL    | `0101` | A << B |
| SHIFTR    | `0110` | A >> B (logical) |
| NOT       | `0111` | ~A |
| (reserved)| `1000` | Reserved |
| (reserved)| `1001` | Reserved |
| SLT       | `1010` | 1 if A < B (signed), else 0 |
| SLTU      | `1011` | 1 if A < B (unsigned), else 0 |
| LOAD      | `1100` | B (or 16-bit constant) |
| LOADHI    | `1101` | {const16, A[15:0]} (load high 16 bits) |
| SHIFTRS   | `1110` | A >> B (arithmetic, sign-extended) |
| (reserved)| `1111` | Reserved |

---

## ARITHM Opcodes (Multi-Cycle)

| Operation | Opcode | Description | Typical Cycles |
|-----------|--------|-------------|----------------|
| MULTS     | `0000` | A × B (signed) | ~4 |
| MULTU     | `0001` | A × B (unsigned) | ~4 |
| MULTFP    | `0010` | A × B (fixed-point) | ~4 |
| DIVS      | `0011` | A ÷ B (signed) | ~32 |
| DIVU      | `0100` | A ÷ B (unsigned) | ~32 |
| DIVFP     | `0101` | A ÷ B (fixed-point) | ~32 |
| MODS      | `0110` | A % B (signed) | ~32 |
| MODU      | `0111` | A % B (unsigned) | ~32 |
| (reserved)| `1000` | Reserved |
| (reserved)| `1001` | Reserved |
| (reserved)| `1010` | Reserved |
| (reserved)| `1011` | Reserved |
| (reserved)| `1100` | Reserved |
| (reserved)| `1101` | Reserved |
| (reserved)| `1110` | Reserved |
| (reserved)| `1111` | Reserved |

---

## Registers

The B32P3 has 16 general-purpose 32-bit registers:

| Register | Name | Description |
|----------|------|-------------|
| R0 | Zero | Hardwired to zero (writes ignored) |
| R1-R14 | General | General-purpose registers |
| R15 | - | General-purpose (often used as return value) |

---

## Addressing Modes

The B32P3 supports the following addressing modes:

1. **Register Direct**: Operand is in a register
2. **Immediate**: 16-bit constant embedded in instruction
3. **Base + Offset**: Memory address = register + signed 16-bit offset
4. **PC-Relative**: Jump/branch target = PC + offset

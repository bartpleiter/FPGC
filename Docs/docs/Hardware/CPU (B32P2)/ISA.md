# ISA

The B32P2 instruction set architecture is a RISC architecture, mostly compatible with the B32P ISA from the FPGC6.
The main differences are support for multi-cycle ARITH instructions. Multiply instructions take now >1 cycles to satisfy timing constraints, meaning MULTS and MULTU are the only instructions from B32P that need different binary patterns on B32P2.

Each instruction is 32 bits and can be one of the following instructions:
``` text
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
15 ARITHM  0  0  1  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
16 ARITHMC 0  0  1  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
15 ARITHC  0  0  0  1||--OPCODE--||----------------16 BIT CONSTANT---------------||--A REG---||--D REG---|
16 ARITH   0  0  0  0||--OPCODE--| x  x  x  x  x  x  x  x  x  x  x  x |--A REG---||--B REG---||--D REG---|
```

1.  `HALT`:    Will prevent the CPU to go to the next instruction by jumping to the same address. Can be interrupted.
2.  `READ`:    Read from memory at address in AREG + (signed) 16 bit offset, store value in DREG.
3.  `WRITE`:   Write value from BREG to memory at address stored in AREG + (signed) 16 bit offset.
4.  `INTID`:   Store the interrupt ID in DREG
5.  `PUSH`:    Pushes value in AREG to stack.
6.  `POP`:     Pops value from stack into DREG.
7.  `JUMP`:    Set PC to 27 bit constant if O is 0. If O is 1, then add the 27 bit constant to PC. 
8.  `JUMPR`:   Set PC to BREG + (signed) 16 bit constant if O is 0. If O is 1, then add the value from BREG + (signed) 16 bit constant to PC. 
9.  `CCACHE`:  Clear all l1 cache by setting all valid bits to 0.
10. `BRANCH`:  Compare AREG to BREG depending on the branch opcode. If S is 1, then used signed comparison. If branch pass, add (signed) 16 bit constant to PC.
11. `SAVPC`:   Save current PC to DREG.
12. `RETI`:    Restore PC after interrupt and re-enable interrupts.
13. `ARITHMC`: Execute multi-cycle operation specified by OPCODE on AREG and (signed) 16 bit constant. Write result to DREG.
14. `ARITHM`:  Execute multi-cycle operation specified by OPCODE on AREG and (signed) 16 bit constant. Write result to DREG.
15. `ARITHC`:  Execute single-cycle operation specified by OPCODE on AREG and (signed) 16 bit constant. Write result to DREG.
16. `ARITH`:   Execute single-cycle operation specified by OPCODE on AREG and BREG. Write result to DREG.

## BRANCH opcodes

The type of branch operation can be specified by the branch opcode:

| Operation | Opcode | Description |
|-----------|--------|-------------|
| BEQ       | 000    | Branch if A == B |
| BGT       | 001    | Branch if A >  B |
| BGE       | 010    | Branch if A >= B |
| XXX       | 011    | Reserved |
| BNE       | 100    | Branch if A != B |
| BLT       | 101    | Branch if A <  B |
| BLE       | 110    | Branch if A <= B |
| XXX       | 111    | Reserved |

Signed comparisons can be enabled by setting the S (sign) bit, creating the BGTS, BGES, BLTS and BLES operations.

## ARITH opcodes

The type of single cycle ALU operation can be specified by the ARITH opcode:

| Operation | Opcode | Description |
|-----------|--------|-------------|
| OR        | 0000   | A OR   B |
| AND       | 0001   | A AND  B |
| XOR       | 0010   | A XOR  B |
| ADD       | 0011   | A  +   B |
| SUB       | 0100   | A  -   B |
| SHIFTL    | 0101   | A  <<  B |
| SHIFTR    | 0110   | A  >>  B |
| NOT(A)    | 0111   | ~A |
| XXX       | 1000   | Reserved [potentially: abs] |
| XXX       | 1001   | Reserved |
| SLT       | 1010   | 1 if A < B, else 0 (signed) |
| SLTU      | 1011   | 1 if A < B, else 0 (unsigned) |
| LOAD      | 1100   | 16 bit constant (or B) |
| LOADHI    | 1101   | {16 bit constant, A[15:0]} (if AREG == DREG, this is equivalent to LOADHI) |
| SHIFTRS   | 1110   | A  >>  B with sign extension |
| XXX       | 1111   | Reserved |

## ARITHM opcodes

The type of multi cycle ALU operation can be specified by the ARITHM opcode:

| Operation | Opcode | Description |
|-----------|--------|-------------|
| MULTS     | 0000   | A  *   B (signed) |
| MULTU     | 0001   | A  *   B (unsigned) |
| MULTFP    | 0010   | A  *   B (signed FP) |
| DIVS      | 0011   | A  /   B (signed) |
| DIVU      | 0100   | A  /   B (unsigned) |
| DIVFP     | 0101   | A  /   B (signed FP) |
| MODS      | 0110   | A  %   B (signed) |
| MODU      | 0111   | A  %   B (unsigned) |
| XXX       | 1000   | Reserved [potentially: FPSQRT] |
| XXX       | 1001   | Reserved [potentially: FPEXP] |
| XXX       | 1010   | Reserved [potentially: FPLOG] |
| XXX       | 1011   | Reserved [potentially: FPSIN] |
| XXX       | 1100   | Reserved [potentially: FPCOS] |
| XXX       | 1101   | Reserved [potentially: FPTAN] |
| XXX       | 1110   | Reserved |
| XXX       | 1111   | Reserved |

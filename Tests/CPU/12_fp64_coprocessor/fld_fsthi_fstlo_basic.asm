Main:
    ; Test fld + fsthi + fstlo: load two CPU regs into FP reg, then extract them back
    ; Load r1=0x12345678, r2=0xABCDEF01
    ; fld f0, r1, r2 -> f0 = {0x12345678, 0xABCDEF01}
    ; fsthi r3, f0 -> r3 = 0x12345678
    ; fstlo r4, f0 -> r4 = 0xABCDEF01
    ; Verify: r3 + r4 should give a known result

    load32 0x00000005 r1     ; high word = 5
    load32 0x00000003 r2     ; low word = 3

    fld r1 r2 r3             ; f3 = {5, 3}  (syntax: fld areg breg dreg -> fd=dreg)

    ; Wait—the assembler syntax is: fld r1 r2 r3
    ; Which maps to: areg=r1, breg=r2, dreg=r3
    ; But for FP operations, dreg[2:0] is the FP register index
    ; r3 in binary is 0011, so fp_dreg = 011 = f3
    ; areg=r1 (CPU reg), breg=r2 (CPU reg)

    fsthi r3 r4 r5           ; fsthi: areg[2:0]=FP reg source, dreg=CPU dest
    ; areg=r3=0011 -> fp_areg=011=f3, dreg=r5
    ; r5 = fp_regs[3][63:32] = high word = 5

    ; Wait, fsthi syntax: fsthi areg breg dreg
    ; areg = fp source register (lower 3 bits)
    ; dreg = CPU destination register
    ; breg = ignored
    ; So: fsthi r3 r0 r5 -> fp_areg=3 (f3), dreg=r5
    ; But assembler requires 3 register args for multi-cycle operations
    ; Let me use: fsthi r3 r0 r5

    ; Actually let me rethink the syntax. The assembler encodes:
    ; ARITHM instruction: [opcode:4][alu_op:4][zeros:12][areg:4][breg:4][dreg:4]
    ; For fsthi: areg is FP source (lower 3 bits), dreg is CPU dest
    ; For fld: areg/breg are CPU sources, dreg is FP dest (lower 3 bits)
    ; The assembler syntax is: mnemonic areg breg dreg, same as mults/multu

    ; Let me try a simpler approach with registers we can verify
    load 42 r1               ; high word = 42
    load 7 r2                ; low word = 7

    ; fld r1 r2 r0 -> fp_dreg = r0[2:0] = 0 -> f0 = {42, 7}
    fld r1 r2 r0

    ; fsthi r0 r0 r3 -> fp_areg = r0[2:0] = 0 -> read f0, dreg = r3
    ; r3 = fp_regs[0][63:32] = 42
    fsthi r0 r0 r3

    ; fstlo r0 r0 r4 -> fp_areg = r0[2:0] = 0 -> read f0, dreg = r4
    ; r4 = fp_regs[0][31:0] = 7
    fstlo r0 r0 r4

    ; Verify: r3 + r4 = 42 + 7 = 49
    add r3 r4 r15            ; expected=49

    halt

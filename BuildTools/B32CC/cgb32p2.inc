/*
 * Originally part of SmallerC
 * Copyright (c) 2012-2015, Alexey Frunze
 * 
 * Modified for FPGC project
 * Copyright (c) 2025 Bart Pleiter
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*****************************************************************************/
/*                                                                           */
/*                                Smaller C                                  */
/*                                                                           */
/*                 A simple and small single-pass C compiler                 */
/*                                                                           */
/*                           B32P2 code generator                            */
/*                                                                           */
/*****************************************************************************/

STATIC
void GenInit(void)
{
  // initialization of target-specific code generator
  // Assembler is responsible to move all non-.code parts away from the code
  SizeOfWord = 4;
  WordAddressable = 1; // B32P2 uses word-addressable memory (each address = 1 word)
  OutputFormat = FormatSegmented;
  CodeHeaderFooter[0] = ".code";
  DataHeaderFooter[0] = ".data";
  RoDataHeaderFooter[0] = ".rdata";
  BssHeaderFooter[0] = ".bss";
  UseLeadingUnderscores = 0;
  FileHeader = ""; // No file header needed for now

}

STATIC
void GenInitFinalize(void)
{
  // finalization of initialization of target-specific code generator
  // This is the place where we put the top of the wrapper code
  //  around the generated code

  printf2(
      ".code\n"
      "; Setup stack and return function before jumping to Main of C program\n"
      "Main:\n"
      "  load32 0 r14            ; initialize base pointer address\n"
      "  load32 0x77FFFF r13     ; initialize main stack address\n"
      "  addr2reg Return_UART r1 ; get address of return function\n"
      "  or r0 r1 r15            ; copy return addr to r15\n"
      "  jump Label_main         ; jump to main of C program\n"
      "                          ; should return to the address in r15\n"
      "  halt                    ; should not get here\n"
      "\n"
      "; Function that is called after Main of C program has returned\n"
      "; Return value should be in r1\n"
      "; Send it over UART and halt afterwards\n"
      "Return_UART:\n"
      "  load32 0x7000000 r2 ; r1 = 0x7000000 | UART tx\n"
      "  write 0 r2 r1       ; write r2 over UART\n"
      "  halt                ; halt\n"
      "\n");
}

STATIC
void GenStartCommentLine(void)
{
  printf2(" ; ");
}

STATIC
void GenWordAlignment(int bss)
{
  (void)bss;
  // B32P2 doesn't need explicit alignment directives for words
}

STATIC
void GenLabel(char* Label, int Static)
{
  {
    // Optional: if in the future linker support is added, below can be used
    // if (!Static && GenExterns)
    //   printf2("  .globl %s\n", Label);
    printf2("Label_%s:\n", Label);
  }
}

STATIC
void GenPrintLabel(char* Label)
{
  {
    if (isdigit(*Label))
      printf2("Label_L%s", Label);
    else
      printf2("Label_%s", Label);
  }
}

STATIC
void GenNumLabel(int Label)
{
  printf2("Label_L%d:\n", Label);
}

STATIC
void GenPrintNumLabel(int label)
{
  printf2("Label_L%d", label);
}

STATIC
void GenZeroData(unsigned Size, int bss)
{
  (void)bss;
  // B32P2: Generate zero words for uninitialized data
  // In word-addressable mode, Size is already in words, not bytes
  unsigned words = WordAddressable ? Size : (Size + 3) / 4;
  unsigned i;
  if (words > 0)
  {
    printf2("  .dw");
    for (i = 0; i < words; i++)
    {
      printf2(" 0");
    }
    printf2("\n");
  }
}

STATIC
void GenIntData(int Size, int Val)
{
  Val = truncInt(Val);
  if (Size == 1 || Size == 2 || Size == 4)
  {
    // B32P2: All data stored as 32-bit words
    printf2("  .dw %d\n", Val);
  }
}

STATIC
void GenStartAsciiString(void)
{
  printf2("  .dsw "); // Each character gets its own 32-bit word (word-addressable)
}

STATIC
void GenAddrData(int Size, char* Label, int ofs)
{
  ofs = truncInt(ofs);
  // B32P2: All data stored as 32-bit words, no need to check size
  printf2("  .dw ");
  GenPrintLabel(Label);
  if (ofs)
    printf2(" %+d", ofs);
  puts2("");
}

STATIC
int GenFxnSizeNeeded(void)
{
  return 0;
}

STATIC
void GenRecordFxnSize(char* startLabelName, int endLabelNo)
{
  (void)startLabelName;
  (void)endLabelNo;
}

#define B32InstrNop     0x00
#define B32InstrHalt    0x01
#define B32InstrSavpc   0x02
#define B32InstrReti    0x03
#define B32InstrRead    0x04
#define B32InstrWrite   0x05
#define B32InstrPush    0x06
#define B32InstrPop     0x07
#define B32InstrAdd     0x08
#define B32InstrSub     0x09
#define B32InstrAnd     0x0A
#define B32InstrOr      0x0B
#define B32InstrXor     0x0D
#define B32InstrNot     0x0E
#define B32InstrShiftL  0x0F
#define B32InstrShiftR  0x10
#define B32InstrShiftRS 0x11
#define B32InstrSlt     0x12
#define B32InstrSltu    0x13
#define B32InstrMults   0x14
#define B32InstrMultu   0x15
#define B32InstrDivs    0x16
#define B32InstrDivu    0x17
#define B32InstrMods    0x18
#define B32InstrModu    0x19
#define B32InstrLoad    0x1A
#define B32InstrLoadhi  0x1B
#define B32InstrLoad32  0x1C
#define B32InstrAddr2reg 0x1D
#define B32InstrBeq     0x1E
#define B32InstrBne     0x1F
#define B32InstrBgt     0x20
#define B32InstrBge     0x21
#define B32InstrBlt     0x22
#define B32InstrBle     0x23
#define B32InstrBgts    0x24
#define B32InstrBges    0x25
#define B32InstrBlts    0x26
#define B32InstrBles    0x27
#define B32InstrJump    0x28
#define B32InstrJumpr   0x29

STATIC
void GenPrintInstr(int instr, int val)
{
  char* p = "";

  (void)val;

  switch (instr)
  {
  case B32InstrNop     : p = "nop"; break;
  case B32InstrHalt    : p = "halt"; break;
  case B32InstrSavpc   : p = "savpc"; break;
  case B32InstrReti    : p = "reti"; break;
  case B32InstrRead    : p = "read"; break;
  case B32InstrWrite   : p = "write"; break;
  case B32InstrPush    : p = "push"; break;
  case B32InstrPop     : p = "pop"; break;
  case B32InstrAdd     : p = "add"; break;
  case B32InstrSub     : p = "sub"; break;
  case B32InstrAnd     : p = "and"; break;
  case B32InstrOr      : p = "or"; break;
  case B32InstrXor     : p = "xor"; break;
  case B32InstrNot     : p = "not"; break;
  case B32InstrShiftL  : p = "shiftl"; break;
  case B32InstrShiftR  : p = "shiftr"; break;
  case B32InstrShiftRS : p = "shiftrs"; break;
  case B32InstrSlt     : p = "slt"; break;
  case B32InstrSltu    : p = "sltu"; break;
  case B32InstrMults   : p = "mults"; break;
  case B32InstrMultu   : p = "multu"; break;
  case B32InstrDivs    : p = "divs"; break;
  case B32InstrDivu    : p = "divu"; break;
  case B32InstrMods    : p = "mods"; break;
  case B32InstrModu    : p = "modu"; break;
  case B32InstrLoad    : p = "load"; break;
  case B32InstrLoadhi  : p = "loadhi"; break;
  case B32InstrLoad32  : p = "load32"; break;
  case B32InstrAddr2reg: p = "addr2reg"; break;
  case B32InstrBeq     : p = "beq"; break;
  case B32InstrBne     : p = "bne"; break;
  case B32InstrBgt     : p = "bgt"; break;
  case B32InstrBge     : p = "bge"; break;
  case B32InstrBlt     : p = "blt"; break;
  case B32InstrBle     : p = "ble"; break;
  case B32InstrBgts    : p = "bgts"; break;
  case B32InstrBges    : p = "bges"; break;
  case B32InstrBlts    : p = "blts"; break;
  case B32InstrBles    : p = "bles"; break;
  case B32InstrJump    : p = "jump"; break;
  case B32InstrJumpr   : p = "jumpr"; break;
  }

  printf2("  %s ", p);
}

#define B32OpReg0                        0x00  // Zero register
#define B32OpReg1                        0x01  // Return value / temp
#define B32OpReg2                        0x02  // Return value / temp
#define B32OpReg3                        0x03  // Temp
#define B32OpReg4                        0x04  // Argument 0
#define B32OpReg5                        0x05  // Argument 1
#define B32OpReg6                        0x06  // Argument 2
#define B32OpReg7                        0x07  // Argument 3
#define B32OpReg8                        0x08  // Temp
#define B32OpReg9                        0x09  // Temp
#define B32OpReg10                       0x0A  // Temp
#define B32OpReg11                       0x0B  // Temp
#define B32OpReg12                       0x0C  // Temp
#define B32OpReg13                       0x0D  // Stack pointer
#define B32OpReg14                       0x0E  // Frame pointer
#define B32OpReg15                       0x0F  // Return address

// Aliases for common register roles
#define B32OpRegZero                     B32OpReg0
#define B32OpRegV0                       B32OpReg1   // Primary return value
#define B32OpRegV1                       B32OpReg2   // Secondary return value
#define B32OpRegA0                       B32OpReg4   // First argument
#define B32OpRegA1                       B32OpReg5   // Second argument
#define B32OpRegA2                       B32OpReg6   // Third argument
#define B32OpRegA3                       B32OpReg7   // Fourth argument
#define B32OpRegT0                       B32OpReg8   // Temp register 0
#define B32OpRegT1                       B32OpReg9   // Temp register 1
#define B32OpRegT2                       B32OpReg10  // Temp register 2
#define B32OpRegT3                       B32OpReg11  // Momentary register 1
#define B32OpRegT4                       B32OpReg12  // Momentary register 2
#define B32OpRegSp                       B32OpReg13  // Stack pointer
#define B32OpRegFp                       B32OpReg14  // Frame pointer
#define B32OpRegRa                       B32OpReg15  // Return address

#define B32OpIndReg0                     0x20
#define B32OpIndReg1                     0x21
#define B32OpIndReg2                     0x22
#define B32OpIndReg3                     0x23
#define B32OpIndReg4                     0x24
#define B32OpIndReg5                     0x25
#define B32OpIndReg6                     0x26
#define B32OpIndReg7                     0x27
#define B32OpIndReg8                     0x28
#define B32OpIndReg9                     0x29
#define B32OpIndReg10                    0x2A
#define B32OpIndReg11                    0x2B
#define B32OpIndReg12                    0x2C
#define B32OpIndReg13                    0x2D
#define B32OpIndReg14                    0x2E
#define B32OpIndReg15                    0x2F

// Aliases for indirect registers
#define B32OpIndRegZero                  B32OpIndReg0
#define B32OpIndRegSp                    B32OpIndReg13
#define B32OpIndRegFp                    B32OpIndReg14
#define B32OpIndRegRa                    B32OpIndReg15

#define B32OpConst                       0x80
#define B32OpLabel                       0x81
#define B32OpNumLabel                    0x82
#define B32OpIndLocal                    B32OpIndRegFp

#define MAX_TEMP_REGS 3 // B32P2: use r8-r10 as temp registers (5 total)
#define TEMP_REG_A B32OpReg11 // use r11 for momentary operations
#define TEMP_REG_B B32OpReg12 // use r12 for momentary operations
#define MAX_STACK_DEPTH 100 // Maximum depth for expression evaluation stack


STATIC
void GenPrintOperand(int op, int val)
{
  if (op >= B32OpReg0 && op <= B32OpReg15)
  {
    printf2("r%d", op);
  }
  else if (op >= B32OpIndReg0 && op <= B32OpIndReg15)
  {
    printf2("%d r%d", truncInt(val), op - B32OpIndReg0);
  }
  else
  {
    switch (op)
    {
    case B32OpConst: printf2("%d", truncInt(val)); break;
    case B32OpLabel: GenPrintLabel(IdentTable + val); break;
    case B32OpNumLabel: GenPrintNumLabel(val); break;

    default:
      errorInternal(100);
      break;
    }
  }
}

STATIC
void GenPrintOperandSeparator(void)
{
  printf2(" ");
}

STATIC
void GenPrintNewLine(void)
{
  puts2("");
}

STATIC
void GenPrintInstr1Operand(int instr, int instrval, int operand, int operandval)
{
  GenPrintInstr(instr, instrval);
  GenPrintOperand(operand, operandval);
  GenPrintNewLine();

}

STATIC
void GenPrintInstr2Operands(int instr, int instrval, int operand1, int operand1val, int operand2, int operand2val)
{
  if (operand2 == B32OpConst && operand2val == 0 &&
      (instr == B32InstrAdd || instr == B32InstrSub))
    return;

  GenPrintInstr(instr, instrval);
  GenPrintOperand(operand1, operand1val);
  GenPrintOperandSeparator();
  GenPrintOperand(operand2, operand2val);
  GenPrintNewLine();
}

STATIC
void GenPrintInstr3Operands(int instr, int instrval,
                            int operand1, int operand1val,
                            int operand2, int operand2val,
                            int operand3, int operand3val)
{
  if (operand3 == B32OpConst && operand3val == 0 &&
      (instr == B32InstrAdd || instr == B32InstrSub) &&
      operand1 == operand2)
    return;

  GenPrintInstr(instr, instrval);
  GenPrintOperand(operand1, operand1val);
  GenPrintOperandSeparator();
  GenPrintOperand(operand2, operand2val);
  GenPrintOperandSeparator();
  GenPrintOperand(operand3, operand3val);
  GenPrintNewLine();

}

STATIC
void GenExtendRegIfNeeded(int reg, int opSz)
{
  // In word-addressable mode, all values are full 32-bit words, no extension needed
  if (WordAddressable)
    return;

  if (opSz == -1)
  {
    // Sign extend byte: shift left 24, then arithmetic right shift 24
    GenPrintInstr3Operands(B32InstrShiftL, 0,
                           reg, 0,
                           B32OpConst, 24,
                           reg, 0);
    GenPrintInstr3Operands(B32InstrShiftRS, 0,
                           reg, 0,
                           B32OpConst, 24,
                           reg, 0);
  }
  else if (opSz == 1)
  {
    // Zero extend byte: mask with 0xFF
    GenPrintInstr3Operands(B32InstrAnd, 0,
                           reg, 0,
                           B32OpConst, 0xFF,
                           reg, 0);
  }
  else if (opSz == -2)
  {
    // Sign extend half: shift left 16, then arithmetic right shift 16
    GenPrintInstr3Operands(B32InstrShiftL, 0,
                           reg, 0,
                           B32OpConst, 16,
                           reg, 0);
    GenPrintInstr3Operands(B32InstrShiftRS, 0,
                           reg, 0,
                           B32OpConst, 16,
                           reg, 0);
  }
  else if (opSz == 2)
  {
    // Zero extend half: mask with 0xFFFF
    GenPrintInstr3Operands(B32InstrAnd, 0,
                           reg, 0,
                           B32OpConst, 0xFFFF,
                           reg, 0);
  }
}

STATIC
int GenIsImm16(int val)
{
  // Check if value fits in 16-bit signed immediate (-32768 to 32767)
  return (val >= -32768 && val <= 32767);
}

STATIC
void GenLoadLargeConstant(int destReg, int val)
{
  // Load a 32-bit constant into a register
  // This uses load32 instruction which can handle full 32-bit values
  GenPrintInstr2Operands(B32InstrLoad32, 0,
                         B32OpConst, val,
                         destReg, 0);
}

STATIC
void GenPrintInstr3OperandsWithLargeImm(int instr, int instrval,
                                         int operand1, int operand1val,
                                         int operand2, int operand2val,
                                         int operand3, int operand3val)
{
  // Handle 3-operand instructions where operand2 might be a large immediate
  // If operand2 is a constant that doesn't fit in 16 bits, load it into a temp register first
  
  if (operand2 == B32OpConst && !GenIsImm16(operand2val))
  {
    // Large immediate - need to load into temp register
    // We use TEMP_REG_B (r12) which is a momentary register
    // We need to save and restore it to be safe
    int tempReg = TEMP_REG_B;
    
    // Save TEMP_REG_B
    GenPrintInstr1Operand(B32InstrPush, 0, tempReg, 0);
    
    // Load the large constant into temp register
    GenLoadLargeConstant(tempReg, operand2val);
    
    // Now use the temp register instead of the immediate
    GenPrintInstr3Operands(instr, instrval,
                          operand1, operand1val,
                          tempReg, 0,
                          operand3, operand3val);
    
    // Restore TEMP_REG_B
    GenPrintInstr1Operand(B32InstrPop, 0, tempReg, 0);
  }
  else
  {
    // Normal case - immediate fits in 16 bits or not a constant
    GenPrintInstr3Operands(instr, instrval,
                          operand1, operand1val,
                          operand2, operand2val,
                          operand3, operand3val);
  }
}

STATIC
void GenJumpUncond(int label)
{
  GenPrintInstr1Operand(B32InstrJump, 0,
                        B32OpNumLabel, label);
}

extern int GenWreg; // GenWreg is defined below

STATIC
void GenJumpIfEqual(int val, int label)
{
  GenPrintInstr2Operands(B32InstrLoad32, 0,
                         B32OpConst, val,
                         TEMP_REG_B, 0);
  GenPrintInstr3Operands(B32InstrBeq, 0,
                         GenWreg, 0,
                         TEMP_REG_B, 0,
                         B32OpNumLabel, label);
}

STATIC
void GenJumpIfZero(int label)
{
#ifndef NO_ANNOTATIONS
  printf2(" ; JumpIfZero\n");
#endif
  GenPrintInstr3Operands(B32InstrBeq, 0,
                         GenWreg, 0,
                         B32OpRegZero, 0,
                         B32OpNumLabel, label);
}

STATIC
void GenJumpIfNotZero(int label)
{
#ifndef NO_ANNOTATIONS
  printf2(" ; JumpIfNotZero\n");
#endif
  GenPrintInstr3Operands(B32InstrBne, 0,
                         GenWreg, 0,
                         B32OpRegZero, 0,
                         B32OpNumLabel, label);
}

fpos_t GenPrologPos;
int GenLeaf;

STATIC
void GenWriteFrameSize(void)
{
  // In word-addressable mode, RA + FP take 2 words (addresses), not 8 bytes
  unsigned size = 2/*RA + FP*/ - CurFxnMinLocalOfs;
  printf2("  sub r13 %10u r13\n", size); // Adjust stack pointer
  printf2("  write %10u r13 r14\n", size - 2); // Save frame pointer
  printf2("  add r13 %10u r14\n", size - 2); // Set up new frame pointer
  printf2("  %cwrite 1 r14 r15\n", GenLeaf ? ';' : ' '); // Save return address if not leaf (offset 1, not 4)
}

STATIC
void GenUpdateFrameSize(void)
{
  fpos_t pos;
  fgetpos(OutFile, &pos);
  fsetpos(OutFile, &GenPrologPos);
  GenWriteFrameSize();
  fsetpos(OutFile, &pos);
}

STATIC
void GenFxnProlog(void)
{
  if (CurFxnParamCntMin && CurFxnParamCntMax)
  {
    int i, cnt = CurFxnParamCntMax;
    int regCnt = cnt;
    if (regCnt > 4)
      regCnt = 4;
    // Save argument registers to stack for local access
    // In word-addressable mode, each parameter is at offset i (not 4 * i)
    for (i = 0; i < regCnt; i++)
      GenPrintInstr3Operands(B32InstrWrite, 0,
                             B32OpConst, i,
                             B32OpRegSp, 0,
                             B32OpRegA0 + i, 0);
    
    // Pop stack-based arguments (beyond the first 4) from hardware stack to memory
    // Arguments are pushed right-to-left, so last arg is pushed first, first extra arg pushed last
    // Therefore, first pop gets arg 5 (index 4), second pop gets arg 6 (index 5), etc.
    if (cnt > 4)
    {
      // Pop from hardware stack and write to memory at sp+4, sp+5, etc.
      for (i = 4; i < cnt; i++)
      {
        GenPrintInstr1Operand(B32InstrPop, 0,
                              TEMP_REG_A, 0);
        GenPrintInstr3Operands(B32InstrWrite, 0,
                               B32OpConst, i,
                               B32OpRegSp, 0,
                               TEMP_REG_A, 0);
      }
    }
  }

  GenLeaf = 1; // will be reset to 0 if a call is generated

  fgetpos(OutFile, &GenPrologPos);
  GenWriteFrameSize();
}

STATIC
void GenGrowStack(int size)
{
  if (!size)
    return;
  GenPrintInstr3Operands(B32InstrSub, 0,
                         B32OpRegSp, 0,
                         B32OpConst, size,
                         B32OpRegSp, 0);
}

STATIC
void GenFxnEpilog(void)
{
  GenUpdateFrameSize();

  // In word-addressable mode, RA is at offset 1 (not 4)
  if (!GenLeaf)
    GenPrintInstr3Operands(B32InstrRead, 0,
                           B32OpConst, 1,
                           B32OpRegFp, 0,
                           B32OpRegRa, 0);

  GenPrintInstr3Operands(B32InstrRead, 0,
                         B32OpConst, 0,
                         B32OpRegFp, 0,
                         B32OpRegFp, 0);

  // In word-addressable mode, RA + FP take 2 words (not 8 bytes)
  GenPrintInstr3Operands(B32InstrAdd, 0,
                         B32OpRegSp, 0,
                         B32OpConst, 2/*RA + FP*/ - CurFxnMinLocalOfs,
                         B32OpRegSp, 0);

  GenPrintInstr2Operands(B32InstrJumpr, 0,
                        B32OpConst, 0,
                        B32OpRegRa, 0);
}

STATIC
int GenMaxLocalsSize(void)
{
  return 0x7FFFFFFF;
}

STATIC
int GenGetBinaryOperatorInstr(int tok)
{
  switch (tok)
  {
  case tokPostAdd:
  case tokAssignAdd:
  case '+':
    return B32InstrAdd;
  case tokPostSub:
  case tokAssignSub:
  case '-':
    return B32InstrSub;
  case '&':
  case tokAssignAnd:
    return B32InstrAnd;
  case '^':
  case tokAssignXor:
    return B32InstrXor;
  case '|':
  case tokAssignOr:
    return B32InstrOr;
  case '<':
  case '>':
  case tokLEQ:
  case tokGEQ:
  case tokEQ:
  case tokNEQ:
  case tokULess:
  case tokUGreater:
  case tokULEQ:
  case tokUGEQ:
    return B32InstrNop;
  case '*':
  case tokAssignMul:
    return B32InstrMults;
  case '/':
  case '%':
  case tokAssignDiv:
  case tokAssignMod:
    return B32InstrDivs;
  case tokUDiv:
  case tokUMod:
  case tokAssignUDiv:
  case tokAssignUMod:
    return B32InstrDivu;
  case tokLShift:
  case tokAssignLSh:
    return B32InstrShiftL;
  case tokRShift:
  case tokAssignRSh:
    return B32InstrShiftRS;
  case tokURShift:
  case tokAssignURSh:
    return B32InstrShiftR;

  default:
    //error("Error: Invalid operator\n");
    errorInternal(101);
    return 0;
  }
}

STATIC
void GenPreIdentAccess(int label)
{
  // B32P2: Load full address into temporary register
  GenPrintInstr2Operands(B32InstrAddr2reg, 0,
                         B32OpLabel, label,
                         TEMP_REG_A, 0);
}

STATIC
void GenPostIdentAccess(void)
{
  // B32P2: No cleanup needed
}

STATIC
void GenReadIdent(int regDst, int opSz, int label)
{
  GenPreIdentAccess(label);
  // B32P2: Read from address in TEMP_REG_A
  GenPrintInstr3Operands(B32InstrRead, 0,
                         B32OpConst, 0,
                         TEMP_REG_A, 0,
                         regDst, 0);
  GenPostIdentAccess();
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenReadLocal(int regDst, int opSz, int ofs)
{
  GenPrintInstr3Operands(B32InstrRead, 0,
                         B32OpConst, ofs,
                         B32OpRegFp, 0,
                         regDst, 0);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenReadIndirect(int regDst, int regSrc, int opSz)
{
  GenPrintInstr3Operands(B32InstrRead, 0,
                         B32OpConst, 0,
                         regSrc, 0,
                         regDst, 0);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenWriteIdent(int regSrc, int opSz, int label)
{
  (void)opSz; // B32P2: All writes are word-sized
  GenPreIdentAccess(label);
  GenPrintInstr3Operands(B32InstrWrite, 0,
                         B32OpConst, 0,
                         TEMP_REG_A, 0,
                         regSrc, 0);
  GenPostIdentAccess();
}

STATIC
void GenWriteLocal(int regSrc, int opSz, int ofs)
{
  (void)opSz; // B32P2: All writes are word-sized
  GenPrintInstr3Operands(B32InstrWrite, 0,
                         B32OpConst, ofs,
                         B32OpRegFp, 0,
                         regSrc, 0);
}

STATIC
void GenWriteIndirect(int regDst, int regSrc, int opSz)
{
  (void)opSz; // B32P2: All writes are word-sized
  GenPrintInstr3Operands(B32InstrWrite, 0,
                         B32OpConst, 0,
                         regDst, 0,
                         regSrc, 0);
}

STATIC
void GenIncDecIdent(int regDst, int opSz, int label, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokInc)
    instr = B32InstrSub;

  GenReadIdent(regDst, opSz, label);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteIdent(regDst, opSz, label);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenIncDecLocal(int regDst, int opSz, int ofs, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokInc)
    instr = B32InstrSub;

  GenReadLocal(regDst, opSz, ofs);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteLocal(regDst, opSz, ofs);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenIncDecIndirect(int regDst, int regSrc, int opSz, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokInc)
    instr = B32InstrSub;

  GenReadIndirect(regDst, regSrc, opSz);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteIndirect(regSrc, regDst, opSz);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenPostIncDecIdent(int regDst, int opSz, int label, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokPostInc)
    instr = B32InstrSub;

  GenReadIdent(regDst, opSz, label);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteIdent(regDst, opSz, label);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, -1,
                         regDst, 0);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenPostIncDecLocal(int regDst, int opSz, int ofs, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokPostInc)
    instr = B32InstrSub;

  GenReadLocal(regDst, opSz, ofs);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteLocal(regDst, opSz, ofs);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, -1,
                         regDst, 0);
  GenExtendRegIfNeeded(regDst, opSz);
}

STATIC
void GenPostIncDecIndirect(int regDst, int regSrc, int opSz, int tok)
{
  int instr = B32InstrAdd;

  if (tok != tokPostInc)
    instr = B32InstrSub;

  GenReadIndirect(regDst, regSrc, opSz);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, 1,
                         regDst, 0);
  GenWriteIndirect(regSrc, regDst, opSz);
  GenPrintInstr3Operands(instr, 0,
                         regDst, 0,
                         B32OpConst, -1,
                         regDst, 0);
  GenExtendRegIfNeeded(regDst, opSz);
}

int CanUseTempRegs;
int TempsUsed;
int StackDepth; // current stack depth for expression evaluation
int GenWreg = B32OpRegV0; // current working register (V0 or Tn or An)
int GenLreg, GenRreg; // left operand register and right operand register after GenPopReg()

/*
  General idea behind GenWreg, GenLreg, GenRreg:

  - In expressions w/o function calls:

    Subexpressions are evaluated in V0, T0, T1, ..., T<MAX_TEMP_REGS-1>. If those registers
    aren't enough, the stack is used additionally.

    The expression result ends up in V0, which is handy for returning from
    functions.

    In the process, GenWreg is the current working register and is one of: V0, T0, T1, ... .
    All unary operators are evaluated in the current working register.

    GenPushReg() and GenPopReg() advance GenWreg as needed when handling binary operators.

    GenPopReg() sets GenWreg, GenLreg and GenRreg. GenLreg and GenRreg are the registers
    where the left and right operands of a binary operator are.

    When the exression runs out of the temporary registers, the stack is used. While it is being
    used, GenWreg remains equal to the last temporary register, and GenPopReg() sets GenLreg = TEMP_REG_A.
    Hence, after GenPopReg() the operands of the binary operator are always in registers and can be
    directly manipulated with.

    Following GenPopReg(), binary operator evaluation must take the left and right operands from
    GenLreg and GenRreg and write the evaluated result into GenWreg. Care must be taken as GenWreg
    will be the same as either GenLreg (when the popped operand comes from T0-T<MAX_TEMP_REGS-1>)
    or GenRreg (when the popped operand comes from the stack in TEMP_REG_A).

  - In expressions with function calls:

    GenWreg is always V0 in subexpressions that aren't function parameters. These subexpressions
    get automatically pushed onto the stack as necessary.

    GenWreg is always V0 in expressions, where return values from function calls are used as parameters
    into other called functions. IOW, this is the case when the function call depth is greater than 1.
    Subexpressions in such expressions get automatically pushed onto the stack as necessary.

    GenWreg is A0-A3 in subexpressions that are function parameters when the function call depth is 1.
    Basically, while a function parameter is evaluated, it's evaluated in the register from where
    the called function will take it. This avoids some of unnecessary register copies and stack
    manipulations in the most simple and very common cases of function calls.
*/

STATIC
void GenWregInc(int inc)
{
  if (inc > 0)
  {
    // Advance the current working register to the next available temporary register
    if (GenWreg == B32OpRegV0)
      GenWreg = B32OpRegT0;
    else
      GenWreg++;
  }
  else
  {
    // Return to the previous current working register
    if (GenWreg == B32OpRegT0)
      GenWreg = B32OpRegV0;
    else
      GenWreg--;
  }
}

STATIC
void GenPushReg(void)
{
  if (CanUseTempRegs && TempsUsed < MAX_TEMP_REGS)
  {
    GenWregInc(1);
    TempsUsed++;
    return;
  }

  // Track stack depth and error when exceeding limit
  if (StackDepth >= MAX_STACK_DEPTH)
    error("Expression too complex, stack depth exceeded\n");

  GenPrintInstr1Operand(B32InstrPush, 0,
                        GenWreg, 0);

  TempsUsed++;

  StackDepth++;
  printf2("; Stack depth: %d\n", StackDepth);
}

STATIC
void GenPopReg(void)
{
  TempsUsed--;

  if (CanUseTempRegs && TempsUsed < MAX_TEMP_REGS)
  {
    GenRreg = GenWreg;
    GenWregInc(-1);
    GenLreg = GenWreg;
    return;
  }  

  GenPrintInstr1Operand(B32InstrPop, 0,
                        TEMP_REG_A, 0);
  GenLreg = TEMP_REG_A;
  GenRreg = GenWreg;

  StackDepth--;
  printf2("; Stack depth: %d\n", StackDepth);
}

#define tokRevIdent    0x100
#define tokRevLocalOfs 0x101
#define tokAssign0     0x102
#define tokNum0        0x103

STATIC
void GenPrep(int* idx)
{
  int tok;
  int oldIdxRight, oldIdxLeft, t0, t1;

  if (*idx < 0)
    //error("GenFuse(): idx < 0\n");
    errorInternal(100);

  tok = stack[*idx][0];

  oldIdxRight = --*idx;

  switch (tok)
  {
  case tokUDiv:
  case tokUMod:
  case tokAssignUDiv:
  case tokAssignUMod:
    if (stack[oldIdxRight][0] == tokNumInt || stack[oldIdxRight][0] == tokNumUint)
    {
      // Change unsigned division to right shift and unsigned modulo to bitwise and
      unsigned m = truncUint(stack[oldIdxRight][1]);
      if (m && !(m & (m - 1)))
      {
        if (tok == tokUMod || tok == tokAssignUMod)
        {
          stack[oldIdxRight][1] = (int)(m - 1);
          tok = (tok == tokUMod) ? '&' : tokAssignAnd;
        }
        else
        {
          t1 = 0;
          while (m >>= 1) t1++;
          stack[oldIdxRight][1] = t1;
          tok = (tok == tokUDiv) ? tokURShift : tokAssignURSh;
        }
        stack[oldIdxRight + 1][0] = tok;
      }
    }
  }

  switch (tok)
  {
  case tokNumUint:
    stack[oldIdxRight + 1][0] = tokNumInt; // reduce the number of cases since tokNumInt and tokNumUint are handled the same way
    // fallthrough
  case tokNumInt:
  case tokNum0:
  case tokIdent:
  case tokLocalOfs:
    break;

  case tokPostAdd:
  case tokPostSub:
  case '-':
  case '/':
  case '%':
  case tokUDiv:
  case tokUMod:
  case tokLShift:
  case tokRShift:
  case tokURShift:
  case tokLogAnd:
  case tokLogOr:
  case tokComma:
    GenPrep(idx);
    // fallthrough
  case tokShortCirc:
  case tokGoto:
  case tokUnaryStar:
  case tokInc:
  case tokDec:
  case tokPostInc:
  case tokPostDec:
  case '~':
  case tokUnaryPlus:
  case tokUnaryMinus:
  case tok_Bool:
  case tokVoid:
  case tokUChar:
  case tokSChar:
  case tokShort:
  case tokUShort:
    GenPrep(idx);
    break;

  case '=':
    if (oldIdxRight + 1 == sp - 1 &&
        (stack[oldIdxRight][0] == tokNumInt || stack[oldIdxRight][0] == tokNumUint) &&
        truncUint(stack[oldIdxRight][1]) == 0)
    {
      // Special case for assigning 0 while throwing away the expression result value
      // TBD??? ,
      stack[oldIdxRight][0] = tokNum0; // this zero constant will not be loaded into a register
      stack[oldIdxRight + 1][0] = tokAssign0; // change '=' to tokAssign0
    }
    // fallthrough
  case tokAssignAdd:
  case tokAssignSub:
  case tokAssignMul:
  case tokAssignDiv:
  case tokAssignUDiv:
  case tokAssignMod:
  case tokAssignUMod:
  case tokAssignLSh:
  case tokAssignRSh:
  case tokAssignURSh:
  case tokAssignAnd:
  case tokAssignXor:
  case tokAssignOr:
    GenPrep(idx);
    oldIdxLeft = *idx;
    GenPrep(idx);
    // If the left operand is an identifier (with static or auto storage), swap it with the right operand
    // and mark it specially, so it can be used directly
    if ((t0 = stack[oldIdxLeft][0]) == tokIdent || t0 == tokLocalOfs)
    {
      t1 = stack[oldIdxLeft][1];
      memmove(stack[oldIdxLeft], stack[oldIdxLeft + 1], (oldIdxRight - oldIdxLeft) * sizeof(stack[0]));
      stack[oldIdxRight][0] = (t0 == tokIdent) ? tokRevIdent : tokRevLocalOfs;
      stack[oldIdxRight][1] = t1;
    }
    break;

  case '+':
  case '*':
  case '&':
  case '^':
  case '|':
  case tokEQ:
  case tokNEQ:
  case '<':
  case '>':
  case tokLEQ:
  case tokGEQ:
  case tokULess:
  case tokUGreater:
  case tokULEQ:
  case tokUGEQ:
    GenPrep(idx);
    oldIdxLeft = *idx;
    GenPrep(idx);
    // If the right operand isn't a constant, but the left operand is, swap the operands
    // so the constant can become an immediate right operand in the instruction
    t1 = stack[oldIdxRight][0];
    t0 = stack[oldIdxLeft][0];
    if (t1 != tokNumInt && t0 == tokNumInt)
    {
      int xor;

      t1 = stack[oldIdxLeft][1];
      memmove(stack[oldIdxLeft], stack[oldIdxLeft + 1], (oldIdxRight - oldIdxLeft) * sizeof(stack[0]));
      stack[oldIdxRight][0] = t0;
      stack[oldIdxRight][1] = t1;

      switch (tok)
      {
      case '<':
      case '>':
        xor = '<' ^ '>'; break;
      case tokLEQ:
      case tokGEQ:
        xor = tokLEQ ^ tokGEQ; break;
      case tokULess:
      case tokUGreater:
        xor = tokULess ^ tokUGreater; break;
      case tokULEQ:
      case tokUGEQ:
        xor = tokULEQ ^ tokUGEQ; break;
      default:
        xor = 0; break;
      }
      tok ^= xor;
    }
    // Handle a few special cases and transform the instruction
    if (stack[oldIdxRight][0] == tokNumInt)
    {
      unsigned m = truncUint(stack[oldIdxRight][1]);
      switch (tok)
      {
      case '*':
        // Change multiplication to left shift, this helps indexing arrays of ints/pointers/etc
        if (m && !(m & (m - 1)))
        {
          t1 = 0;
          while (m >>= 1) t1++;
          stack[oldIdxRight][1] = t1;
          tok = tokLShift;
        }
        break;
      case tokLEQ:
        // left <= const will later change to left < const+1, but const+1 must be <=0x7FFFFFFF
        if (m == 0x7FFFFFFF)
        {
          // left <= 0x7FFFFFFF is always true, change to the equivalent left >= 0u
          stack[oldIdxRight][1] = 0;
          tok = tokUGEQ;
        }
        break;
      case tokULEQ:
        // left <= const will later change to left < const+1, but const+1 must be <=0xFFFFFFFFu
        if (m == 0xFFFFFFFF)
        {
          // left <= 0xFFFFFFFFu is always true, change to the equivalent left >= 0u
          stack[oldIdxRight][1] = 0;
          tok = tokUGEQ;
        }
        break;
      case '>':
        // left > const will later change to !(left < const+1), but const+1 must be <=0x7FFFFFFF
        if (m == 0x7FFFFFFF)
        {
          // left > 0x7FFFFFFF is always false, change to the equivalent left & 0
          stack[oldIdxRight][1] = 0;
          tok = '&';
        }
        break;
      case tokUGreater:
        // left > const will later change to !(left < const+1), but const+1 must be <=0xFFFFFFFFu
        if (m == 0xFFFFFFFF)
        {
          // left > 0xFFFFFFFFu is always false, change to the equivalent left & 0
          stack[oldIdxRight][1] = 0;
          tok = '&';
        }
        break;
      }
    }
    stack[oldIdxRight + 1][0] = tok;
    break;

  case ')':
    while (stack[*idx][0] != '(')
    {
      GenPrep(idx);
      if (stack[*idx][0] == ',')
        --*idx;
    }
    --*idx;
    break;

  default:
    //error("GenPrep: unexpected token %s\n", GetTokenName(tok));
    errorInternal(101);
  }
}

/*
B32P2 Comparison Code Generation Strategy:
;     l <[u] 0       // slt[u] w, w, 0                            "k"
      l <[u] const   // slt[u] w, w, const                        "m"
      l <[u] r       // slt[u] w, l, r                            "i"
* if (l <    0)      // bges w, $0, Lskip                         "f"
  if (l <[u] const)  // slt[u] w, w, const; beq w, $0, Lskip      "mc"
  if (l <[u] r)      // slt[u] w, l, r; beq w, $0, Lskip          "ic"

;     l <=[u] 0      // slt[u] w, w, 1                            "l"
      l <=[u] const  // slt[u] w, w, const + 1                    "n"
      l <=[u] r      // slt[u] w, r, l; xor w, w, 1               "js"
* if (l <=    0)     // bgts w, $0, Lskip                         "g"
  if (l <=[u] const) // slt[u] w, w, const + 1; beq w, $0, Lskip  "nc"
  if (l <=[u] r)     // slt[u] w, r, l; bne w, $0, Lskip          "jd"

      l >[u] 0       // slt[u] w, $0, w                           "o"
      l >[u] const   // slt[u] w, w, const + 1; xor w, w, 1       "ns"
      l >[u] r       // slt[u] w, r, l                            "j"
* if (l >    0)      // bles w, $0, Lskip                         "h"
**if (l >u   0)      // beq w, $0, Lskip
  if (l >[u] const)  // slt[u] w, w, const + 1; bne w, $0, Lskip  "nd"
  if (l >[u] r)      // slt[u] w, r, l; beq w, $0, Lskip          "jc"

;     l >=[u] 0      // slt[u] w, w, 0; xor w, w, 1               "ks"
      l >=[u] const  // slt[u] w, w, const; xor w, w, 1           "ms"
      l >=[u] r      // slt[u] w, l, r; xor w, w, 1               "is"
* if (l >=    0)     // blts w, $0, Lskip                         "e"
  if (l >=[u] const) // slt[u] w, w, const; bne w, $0, Lskip      "md"
  if (l >=[u] r)     // slt[u] w, l, r; bne w, $0, Lskip          "id"

      l == 0         // sltu w, w, 1                              "q"
      l == const     // xor w, w, const; sltu w, w, 1             "tq"
      l == r         // xor w, l, r; sltu w, w, 1                 "rq"
  if (l == 0)        // bne w, $0, Lskip                          "d"
  if (l == const)    // xor w, w, const; bne w, $0, Lskip         "td"
  if (l == r)        // bne l, r, Lskip                           "b"

      l != 0         // sltu w, $0, w                             "p"
      l != const     // xor w, w, const; sltu w, $0, w            "tp"
      l != r         // xor w, l, r; sltu w, $0, w                "rp"
  if (l != 0)        // beq w, $0, Lskip                          "c"
  if (l != const)    // xor w, w, const; beq w, $0, Lskip         "tc"
  if (l != r)        // beq l, r, Lskip                           "a"
*/
char CmpBlocks[6/*op*/][2/*condbranch*/][3/*constness*/][2] =
{
  {
    { "k", "m", "i" },
    { "f", "mc", "ic" }
  },
  {
    { "l", "n", "js" },
    { "g", "nc", "jd" }
  },
  {
    { "o", "ns", "j" },
    { "h", "nd", "jc" }
  },
  {
    { "ks", "ms", "is" },
    { "e", "md", "id" }
  },
  {
    { "q", "tq", "rq" },
    { "d", "td", "b" }
  },
  {
    { "p", "tp", "rp" },
    { "c", "tc", "a" }
  }
};

STATIC
void GenCmp(int* idx, int op)
{
  // constness: 0 = zero const, 1 = non-zero const, 2 = non-const
  int constness = (stack[*idx - 1][0] == tokNumInt) ? (stack[*idx - 1][1] != 0) : 2;
  int constval = (constness == 1) ? truncInt(stack[*idx - 1][1]) : 0;
  // condbranch: 0 = no conditional branch, 1 = branch if true, 2 = branch if false
  int condbranch = (*idx + 1 < sp) ? (stack[*idx + 1][0] == tokIf) + (stack[*idx + 1][0] == tokIfNot) * 2 : 0;
  int unsign = op >> 4;
  int slt = unsign ? B32InstrSltu : B32InstrSlt;
  int label = condbranch ? stack[*idx + 1][1] : 0;
  char* p;
  int i;

  op &= 0xF;
  if (constness == 2)
    GenPopReg();

  // B32P2: bgts, bges, blts, bles are for signed comparison with 0 only.
  // For unsigned conditional branches on <0u, <=0u, >0u, >=0u, use the general method
  // (slt/sltu + beq/bne) instead since B32P2 doesn't have unsigned zero-comparison branches.
  if (condbranch && op < 4 && constness == 0 && unsign)
  {
    // Except, >0u is more optimal as !=0
    if (op == 2)
      op = 5;
    else
      constness = 1;
  }

  p = CmpBlocks[op][condbranch != 0][constness];

  for (i = 0; i < 2; i++)
  {
    switch (p[i])
    {
    case 'a':
      condbranch ^= 3;
      // fallthrough
    case 'b':
      GenPrintInstr3Operands((condbranch == 1) ? B32InstrBeq : B32InstrBne, 0,
                             GenLreg, 0,
                             GenRreg, 0,
                             B32OpNumLabel, label);
      break;
    case 'c':
      condbranch ^= 3;
      // fallthrough
    case 'd':
      GenPrintInstr3Operands((condbranch == 1) ? B32InstrBeq : B32InstrBne, 0,
                             GenWreg, 0,
                             B32OpRegZero, 0,
                             B32OpNumLabel, label);
      break;
    case 'e':
      condbranch ^= 3;
      // fallthrough
    case 'f':
      // B32P2: Branch if less than zero (signed)
      // if (l < 0): blts GenWreg, r0, label
      // if (l >= 0): bges GenWreg, r0, label
      GenPrintInstr3Operands((condbranch == 1) ? B32InstrBlts : B32InstrBges, 0,
                             GenWreg, 0,
                             B32OpRegZero, 0,
                             B32OpNumLabel, label);
      break;
    case 'g':
      condbranch ^= 3;
      // fallthrough
    case 'h':
      // B32P2: Branch if greater than zero (signed)
      // if (l > 0): bgts GenWreg, r0, label
      // if (l <= 0): bles GenWreg, r0, label
      GenPrintInstr3Operands((condbranch == 1) ? B32InstrBgts : B32InstrBles, 0,
                             GenWreg, 0,
                             B32OpRegZero, 0,
                             B32OpNumLabel, label);
      break;
    case 'i':
      GenPrintInstr3Operands(slt, 0,
                             GenLreg, 0,
                             GenRreg, 0,
                             GenWreg, 0);
      break;
    case 'j':
      GenPrintInstr3Operands(slt, 0,
                             GenRreg, 0,
                             GenLreg, 0,
                             GenWreg, 0);
      break;
    case 'k':
      GenPrintInstr3Operands(slt, 0,
                             GenWreg, 0,
                             B32OpRegZero, 0,
                             GenWreg, 0);
      break;
    case 'l':
      GenPrintInstr3Operands(slt, 0,
                             GenWreg, 0,
                             B32OpConst, 1,
                             GenWreg, 0);
      break;
    case 'n':
      constval++;
      // fallthrough
    case 'm':
      GenPrintInstr3OperandsWithLargeImm(slt, 0,
                             GenWreg, 0,
                             B32OpConst, constval,
                             GenWreg, 0);
      break;
    case 'o':
      GenPrintInstr3Operands(slt, 0,
                             B32OpRegZero, 0,
                             GenWreg, 0,
                             GenWreg, 0);
      break;
    case 'p':
      GenPrintInstr3Operands(B32InstrSltu, 0,
                             B32OpRegZero, 0,
                             GenWreg, 0,
                             GenWreg, 0);
      break;
    case 'q':
      GenPrintInstr3Operands(B32InstrSltu, 0,
                             GenWreg, 0,
                             B32OpConst, 1,
                             GenWreg, 0);
      break;
    case 'r':
      GenPrintInstr3Operands(B32InstrXor, 0,
                             GenLreg, 0,
                             GenRreg, 0,
                             GenWreg, 0);
      break;
    case 's':
      GenPrintInstr3Operands(B32InstrXor, 0,
                             GenWreg, 0,
                             B32OpConst, 1,
                             GenWreg, 0);
      break;
    case 't':
      GenPrintInstr3OperandsWithLargeImm(B32InstrXor, 0,
                             GenWreg, 0,
                             B32OpConst, constval,
                             GenWreg, 0);
      break;
    }
  }

  *idx += condbranch != 0;
}

STATIC
int GenIsCmp(int t)
{
  return
    t == '<' ||
    t == '>' ||
    t == tokGEQ ||
    t == tokLEQ ||
    t == tokULess ||
    t == tokUGreater ||
    t == tokUGEQ ||
    t == tokULEQ ||
    t == tokEQ ||
    t == tokNEQ;
}

// Improved register/stack-based code generator
// DONE: test 32-bit code generation
STATIC
void GenExpr0(void)
{
  int i;
  int gotUnary = 0;
  int maxCallDepth = 0;
  int callDepth = 0;
  int paramOfs = 0;
  int t = sp - 1;

  if (stack[t][0] == tokIf || stack[t][0] == tokIfNot || stack[t][0] == tokReturn)
    t--;
  GenPrep(&t);

  for (i = 0; i < sp; i++)
    if (stack[i][0] == '(')
    {
      if (++callDepth > maxCallDepth)
        maxCallDepth = callDepth;
    }
    else if (stack[i][0] == ')')
    {
      callDepth--;
    }

  CanUseTempRegs = maxCallDepth == 0;
  TempsUsed = 0;
  StackDepth = 0;
  if (GenWreg != B32OpRegV0)
    errorInternal(102);

  for (i = 0; i < sp; i++)
  {
    int tok = stack[i][0];
    int v = stack[i][1];

#ifndef NO_ANNOTATIONS
    switch (tok)
    {
    case tokNumInt: printf2(" ; %d\n", truncInt(v)); break;
    //case tokNumUint: printf2(" ; %uu\n", truncUint(v)); break;
    case tokIdent: case tokRevIdent: printf2(" ; %s\n", IdentTable + v); break;
    case tokLocalOfs: case tokRevLocalOfs: printf2(" ; local ofs\n"); break;
    case ')': printf2(" ; ) fxn call\n"); break;
    case tokUnaryStar: printf2(" ; * (read dereference)\n"); break;
    case '=': printf2(" ; = (write dereference)\n"); break;
    case tokShortCirc: printf2(" ; short-circuit "); break;
    case tokGoto: printf2(" ; sh-circ-goto "); break;
    case tokLogAnd: printf2(" ; short-circuit && target\n"); break;
    case tokLogOr: printf2(" ; short-circuit || target\n"); break;
    case tokIf: case tokIfNot: case tokReturn: break;
    case tokNum0: printf2(" ; 0\n"); break;
    case tokAssign0:  printf2(" ; =\n"); break;
    default: printf2(" ; %s\n", GetTokenName(tok)); break;
    }
#endif

    switch (tok)
    {
    case tokNumInt:
      if (!(i + 1 < sp && ((t = stack[i + 1][0]) == '+' ||
                           t == '-' ||
                           t == '&' ||
                           t == '^' ||
                           t == '|' ||
                           t == tokLShift ||
                           t == tokRShift ||
                           t == tokURShift ||
                           GenIsCmp(t))))
      {
        if (gotUnary)
          GenPushReg();

        GenPrintInstr2Operands(B32InstrLoad32, 0,
                               B32OpConst, v,
                               GenWreg, 0);
      }
      gotUnary = 1;
      break;

    case tokIdent:
      if (gotUnary)
        GenPushReg();
      if (!(i + 1 < sp && ((t = stack[i + 1][0]) == ')' ||
                           t == tokUnaryStar ||
                           t == tokInc ||
                           t == tokDec ||
                           t == tokPostInc ||
                           t == tokPostDec)))
      {
        GenPrintInstr2Operands(B32InstrAddr2reg, 0,
                               B32OpLabel, v,
                               GenWreg, 0);
      }
      gotUnary = 1;
      break;

    case tokLocalOfs:
      if (gotUnary)
        GenPushReg();
      if (!(i + 1 < sp && ((t = stack[i + 1][0]) == tokUnaryStar ||
                           t == tokInc ||
                           t == tokDec ||
                           t == tokPostInc ||
                           t == tokPostDec)))
      {
        GenPrintInstr3Operands(B32InstrAdd, 0,
                               B32OpRegFp, 0,
                               B32OpConst, v,
                               GenWreg, 0);
      }
      gotUnary = 1;
      break;

    case '(':
      if (gotUnary)
        GenPushReg();
      gotUnary = 0;
      // In word-addressable mode, v is count of args, max 4 args in registers
      // Allocate stack space for parameters if needed
      if (maxCallDepth != 1 && v < 4)
        GenGrowStack(4 - v);
      paramOfs = v - 1;
      if (maxCallDepth == 1 && paramOfs >= 0 && paramOfs <= 3)
      {
        // Work directly in A0-A3 instead of working in V0 and avoid copying V0 to A0-A3
        GenWreg = B32OpRegA0 + paramOfs;
      }
      break;

    case ',':
      if (maxCallDepth == 1)
      {
        if (paramOfs == 4)
        {
          // Got the last on-stack parameter, the rest will go in A0-A3
          GenPushReg();
          gotUnary = 0;
          GenWreg = B32OpRegA3;
        }
        if (paramOfs >= 0 && paramOfs <= 3)
        {
          // Advance to the next An reg or revert to V0
          if (paramOfs)
            GenWreg--;
          else
            GenWreg = B32OpRegV0;
          gotUnary = 0;
        }
        paramOfs -= 1;
      }
      break;

    case ')':
      GenLeaf = 0;
      if (maxCallDepth != 1)
      {
        // In word-addressable mode, v is count of args (1, 2, 3, 4, ...)
        if (v >= 1)
          GenPrintInstr3Operands(B32InstrRead, 0,
                                 B32OpConst, 0,
                                 B32OpRegSp, 0,
                                 B32OpRegA0, 0);
        if (v >= 2)
          GenPrintInstr3Operands(B32InstrRead, 0,
                                 B32OpConst, 1,
                                 B32OpRegSp, 0,
                                 B32OpRegA1, 0);
        if (v >= 3)
          GenPrintInstr3Operands(B32InstrRead, 0,
                                 B32OpConst, 2,
                                 B32OpRegSp, 0,
                                 B32OpRegA2, 0);
        if (v >= 4)
          GenPrintInstr3Operands(B32InstrRead, 0,
                                 B32OpConst, 3,
                                 B32OpRegSp, 0,
                                 B32OpRegA3, 0);
      }
      else
      {
        // Allocate at least 4 words, or v words if more than 4 arguments
        GenGrowStack(v > 4 ? v : 4);
      }
      if (stack[i - 1][0] == tokIdent)
      {
        // B32P2: Save return address (PC + offset) and jump to function
        GenPrintInstr1Operand(B32InstrSavpc, 0,
                              B32OpRegRa, 0);
        GenPrintInstr3Operands(B32InstrAdd, 0,
                             B32OpRegRa, 0,
                             B32OpConst, 3,
                             B32OpRegRa, 0);
        GenPrintInstr1Operand(B32InstrJump, 0,
                              B32OpLabel, stack[i - 1][1]);
      }
      else
      {
        // B32P2: Save return address (PC + offset) and jump to register
        GenPrintInstr1Operand(B32InstrSavpc, 0,
                              B32OpRegRa, 0);
        GenPrintInstr3Operands(B32InstrAdd, 0,
                             B32OpRegRa, 0,
                             B32OpConst, 3,
                             B32OpRegRa, 0);
        GenPrintInstr2Operands(B32InstrJumpr, 0,
                              B32OpConst, 0,
                              GenWreg, 0);
      }
      // In word-addressable mode, always allocate at least 4 words for args
      if (v < 4)
        v = 4;
      GenGrowStack(-v);
      break;

    case tokUnaryStar:
      if (stack[i - 1][0] == tokIdent)
        GenReadIdent(GenWreg, v, stack[i - 1][1]);
      else if (stack[i - 1][0] == tokLocalOfs)
        GenReadLocal(GenWreg, v, stack[i - 1][1]);
      else
        GenReadIndirect(GenWreg, GenWreg, v);
      break;

    case tokUnaryPlus:
      break;
    case '~':
      GenPrintInstr2Operands(B32InstrNot, 0,
                             GenWreg, 0,
                             GenWreg, 0);
      break;
    case tokUnaryMinus:
      GenPrintInstr3Operands(B32InstrSub, 0,
                             B32OpRegZero, 0,
                             GenWreg, 0,
                             GenWreg, 0);
      break;

    case '+':
    case '-':
    case '*':
    case '&':
    case '^':
    case '|':
    case tokLShift:
    case tokRShift:
    case tokURShift:
      if (stack[i - 1][0] == tokNumInt && tok != '*')
      {
        int instr = GenGetBinaryOperatorInstr(tok);
        GenPrintInstr3OperandsWithLargeImm(instr, 0,
                               GenWreg, 0,
                               B32OpConst, stack[i - 1][1],
                               GenWreg, 0);
      }
      else
      {
        int instr = GenGetBinaryOperatorInstr(tok);
        GenPopReg();
        GenPrintInstr3Operands(instr, 0,
                               GenLreg, 0,
                               GenRreg, 0,
                               GenWreg, 0);
      }
      break;

    case '/':
    case tokUDiv:
    case '%':
    case tokUMod:
      {
        GenPopReg();
        if (tok == '/')
          GenPrintInstr3Operands(B32InstrDivs, 0,
                                 GenLreg, 0,
                                 GenRreg, 0,
                                 GenWreg, 0);
        else if (tok == tokUDiv)
          GenPrintInstr3Operands(B32InstrDivu, 0,
                                 GenLreg, 0,
                                 GenRreg, 0,
                                 GenWreg, 0);
        else if (tok == '%')
          GenPrintInstr3Operands(B32InstrMods, 0,
                                 GenLreg, 0,
                                 GenRreg, 0,
                                 GenWreg, 0);
        else if (tok == tokUMod)
          GenPrintInstr3Operands(B32InstrModu, 0,
                                 GenLreg, 0,
                                 GenRreg, 0,
                                 GenWreg, 0);
      }
      break;

    case tokInc:
    case tokDec:
      if (stack[i - 1][0] == tokIdent)
      {
        GenIncDecIdent(GenWreg, v, stack[i - 1][1], tok);
      }
      else if (stack[i - 1][0] == tokLocalOfs)
      {
        GenIncDecLocal(GenWreg, v, stack[i - 1][1], tok);
      }
      else
      {
        GenPrintInstr3Operands(B32InstrOr, 0,
                               GenWreg, 0,
                               B32OpRegZero, 0,
                               TEMP_REG_A, 0);
        GenIncDecIndirect(GenWreg, TEMP_REG_A, v, tok);
      }
      break;
    case tokPostInc:
    case tokPostDec:
      if (stack[i - 1][0] == tokIdent)
      {
        GenPostIncDecIdent(GenWreg, v, stack[i - 1][1], tok);
      }
      else if (stack[i - 1][0] == tokLocalOfs)
      {
        GenPostIncDecLocal(GenWreg, v, stack[i - 1][1], tok);
      }
      else
      {
        GenPrintInstr3Operands(B32InstrOr, 0,
                               GenWreg, 0,
                               B32OpRegZero, 0,
                               TEMP_REG_A, 0);
        GenPostIncDecIndirect(GenWreg, TEMP_REG_A, v, tok);
      }
      break;

    case tokPostAdd:
    case tokPostSub:
      {
        int instr = GenGetBinaryOperatorInstr(tok);
        GenPopReg();
        if (GenWreg == GenLreg)
        {
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenLreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);

          GenReadIndirect(GenWreg, TEMP_REG_B, v);
          GenPrintInstr3Operands(instr, 0,
                                 GenWreg, 0,
                                 GenRreg, 0,
                                 TEMP_REG_A, 0);
          GenWriteIndirect(TEMP_REG_B, TEMP_REG_A, v);
        }
        else
        {
          // GenWreg == GenRreg here
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenRreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);

          GenReadIndirect(GenWreg, GenLreg, v);
          GenPrintInstr3Operands(instr, 0,
                                 GenWreg, 0,
                                 TEMP_REG_B, 0,
                                 TEMP_REG_B, 0);
          GenWriteIndirect(GenLreg, TEMP_REG_B, v);
        }
      }
      break;

    case tokAssignAdd:
    case tokAssignSub:
    case tokAssignMul:
    case tokAssignAnd:
    case tokAssignXor:
    case tokAssignOr:
    case tokAssignLSh:
    case tokAssignRSh:
    case tokAssignURSh:
      if (stack[i - 1][0] == tokRevLocalOfs || stack[i - 1][0] == tokRevIdent)
      {
        int instr = GenGetBinaryOperatorInstr(tok);

        if (stack[i - 1][0] == tokRevLocalOfs)
          GenReadLocal(TEMP_REG_B, v, stack[i - 1][1]);
        else
          GenReadIdent(TEMP_REG_B, v, stack[i - 1][1]);

        GenPrintInstr3Operands(instr, 0,
                               TEMP_REG_B, 0,
                               GenWreg, 0,
                               GenWreg, 0);

        if (stack[i - 1][0] == tokRevLocalOfs)
          GenWriteLocal(GenWreg, v, stack[i - 1][1]);
        else
          GenWriteIdent(GenWreg, v, stack[i - 1][1]);
      }
      else
      {
        int instr = GenGetBinaryOperatorInstr(tok);
        int lsaved, rsaved;
        GenPopReg();
        if (GenWreg == GenLreg)
        {
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenLreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);
          lsaved = TEMP_REG_B;
          rsaved = GenRreg;
        }
        else
        {
          // GenWreg == GenRreg here
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenRreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);
          rsaved = TEMP_REG_B;
          lsaved = GenLreg;
        }

        GenReadIndirect(GenWreg, GenLreg, v); // destroys either GenLreg or GenRreg because GenWreg coincides with one of them
        GenPrintInstr3Operands(instr, 0,
                               GenWreg, 0,
                               rsaved, 0,
                               GenWreg, 0);
        GenWriteIndirect(lsaved, GenWreg, v);
      }
      GenExtendRegIfNeeded(GenWreg, v);
      break;

    case tokAssignDiv:
    case tokAssignUDiv:
    case tokAssignMod:
    case tokAssignUMod:
      if (stack[i - 1][0] == tokRevLocalOfs || stack[i - 1][0] == tokRevIdent)
      {
        if (stack[i - 1][0] == tokRevLocalOfs)
          GenReadLocal(TEMP_REG_B, v, stack[i - 1][1]);
        else
          GenReadIdent(TEMP_REG_B, v, stack[i - 1][1]);

        if (tok == tokAssignDiv)
          GenPrintInstr3Operands(B32InstrDivs, 0,
                                 TEMP_REG_B, 0,
                                 GenWreg, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignUDiv)
          GenPrintInstr3Operands(B32InstrDivu, 0,
                                 TEMP_REG_B, 0,
                                 GenWreg, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignMod)
          GenPrintInstr3Operands(B32InstrMods, 0,
                                 TEMP_REG_B, 0,
                                 GenWreg, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignUMod)
          GenPrintInstr3Operands(B32InstrModu, 0,
                                 TEMP_REG_B, 0,
                                 GenWreg, 0,
                                 GenWreg, 0);

        if (stack[i - 1][0] == tokRevLocalOfs)
          GenWriteLocal(GenWreg, v, stack[i - 1][1]);
        else
          GenWriteIdent(GenWreg, v, stack[i - 1][1]);
      }
      else
      {
        int lsaved, rsaved;
        GenPopReg();
        if (GenWreg == GenLreg)
        {
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenLreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);
          lsaved = TEMP_REG_B;
          rsaved = GenRreg;
        }
        else
        {
          // GenWreg == GenRreg here
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenRreg, 0,
                                 B32OpRegZero, 0,
                                 TEMP_REG_B, 0);
          rsaved = TEMP_REG_B;
          lsaved = GenLreg;
        }

        GenReadIndirect(GenWreg, GenLreg, v); // destroys either GenLreg or GenRreg because GenWreg coincides with one of them
        if (tok == tokAssignDiv)
          GenPrintInstr3Operands(B32InstrDivs, 0,
                                 GenWreg, 0,
                                 rsaved, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignUDiv)
          GenPrintInstr3Operands(B32InstrDivu, 0,
                                 GenWreg, 0,
                                 rsaved, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignMod)
          GenPrintInstr3Operands(B32InstrMods, 0,
                                 GenWreg, 0,
                                 rsaved, 0,
                                 GenWreg, 0);
        else if (tok == tokAssignUMod)
          GenPrintInstr3Operands(B32InstrModu, 0,
                                 GenWreg, 0,
                                 rsaved, 0,
                                 GenWreg, 0);
        GenWriteIndirect(lsaved, GenWreg, v);
      }
      GenExtendRegIfNeeded(GenWreg, v);
      break;

    case '=':
      if (stack[i - 1][0] == tokRevLocalOfs)
      {
        GenWriteLocal(GenWreg, v, stack[i - 1][1]);
      }
      else if (stack[i - 1][0] == tokRevIdent)
      {
        GenWriteIdent(GenWreg, v, stack[i - 1][1]);
      }
      else
      {
        GenPopReg();
        GenWriteIndirect(GenLreg, GenRreg, v);
        if (GenWreg != GenRreg)
          GenPrintInstr3Operands(B32InstrOr, 0,
                                 GenRreg, 0,
                                 B32OpRegZero, 0,
                                 GenWreg, 0);
      }
      GenExtendRegIfNeeded(GenWreg, v);
      break;

    case tokAssign0: // assignment of 0, while throwing away the expression result value
      if (stack[i - 1][0] == tokRevLocalOfs)
      {
        GenWriteLocal(B32OpRegZero, v, stack[i - 1][1]);
      }
      else if (stack[i - 1][0] == tokRevIdent)
      {
        GenWriteIdent(B32OpRegZero, v, stack[i - 1][1]);
      }
      else
      {
        GenWriteIndirect(GenWreg, B32OpRegZero, v);
      }
      break;

    case '<':         GenCmp(&i, 0x00); break;
    case tokLEQ:      GenCmp(&i, 0x01); break;
    case '>':         GenCmp(&i, 0x02); break;
    case tokGEQ:      GenCmp(&i, 0x03); break;
    case tokULess:    GenCmp(&i, 0x10); break;
    case tokULEQ:     GenCmp(&i, 0x11); break;
    case tokUGreater: GenCmp(&i, 0x12); break;
    case tokUGEQ:     GenCmp(&i, 0x13); break;
    case tokEQ:       GenCmp(&i, 0x04); break;
    case tokNEQ:      GenCmp(&i, 0x05); break;

    case tok_Bool:
      GenPrintInstr3Operands(B32InstrSltu, 0,
                             B32OpRegZero, 0,
                             GenWreg, 0,
                             GenWreg, 0);
      break;

    case tokSChar:
      // Sign extend byte using shift operations
      GenPrintInstr3Operands(B32InstrShiftL, 0,
                             GenWreg, 0,
                             B32OpConst, 24,
                             GenWreg, 0);
      GenPrintInstr3Operands(B32InstrShiftRS, 0,
                             GenWreg, 0,
                             B32OpConst, 24,
                             GenWreg, 0);
      break;
    case tokUChar:
      GenPrintInstr3Operands(B32InstrAnd, 0,
                             GenWreg, 0,
                             B32OpConst, 0xFF,
                             GenWreg, 0);
      break;
    case tokShort:
      // Sign extend half using shift operations
      GenPrintInstr3Operands(B32InstrShiftL, 0,
                             GenWreg, 0,
                             B32OpConst, 16,
                             GenWreg, 0);
      GenPrintInstr3Operands(B32InstrShiftRS, 0,
                             GenWreg, 0,
                             B32OpConst, 16,
                             GenWreg, 0);
      break;
    case tokUShort:
      GenPrintInstr3Operands(B32InstrAnd, 0,
                             GenWreg, 0,
                             B32OpConst, 0xFFFF,
                             GenWreg, 0);
      break;

    case tokShortCirc:
#ifndef NO_ANNOTATIONS
      if (v >= 0)
        printf2("&&\n");
      else
        printf2("||\n");
#endif
      if (v >= 0)
        GenJumpIfZero(v); // &&
      else
        GenJumpIfNotZero(-v); // ||
      gotUnary = 0;
      break;
    case tokGoto:
#ifndef NO_ANNOTATIONS
      printf2("goto\n");
#endif
      GenJumpUncond(v);
      gotUnary = 0;
      break;
    case tokLogAnd:
    case tokLogOr:
      GenNumLabel(v);
      break;

    case tokVoid:
      gotUnary = 0;
      break;

    case tokRevIdent:
    case tokRevLocalOfs:
    case tokComma:
    case tokReturn:
    case tokNum0:
      break;

    case tokIf:
      GenJumpIfNotZero(stack[i][1]);
      break;
    case tokIfNot:
      GenJumpIfZero(stack[i][1]);
      break;

    default:
      //error("Error: Internal Error: GenExpr0(): unexpected token %s\n", GetTokenName(tok));
      errorInternal(103);
      break;
    }
  }

  if (GenWreg != B32OpRegV0)
    errorInternal(104);
}

STATIC
void GenDumpChar(int ch)
{
  if (ch < 0)
  {
    if (TokenStringLen)
      printf2("\"\n");
    return;
  }

  if (TokenStringLen == 0)
  {
    GenStartAsciiString();
    printf2("\"");
  }

  if (ch >= 0x20 && ch <= 0x7E)
  {
    if (ch == '"' || ch == '\\')
      printf2("\\");
    printf2("%c", ch);
  }
  else
  {
    printf2("\\%03o", ch);
  }
}

STATIC
void GenExpr(void)
{
  GenExpr0();
}

STATIC
void GenFin(void)
{
  // To be honest, I am not sure what this does and just converted it to B32P2 assembly
  if (StructCpyLabel)
  {
    int lbl = LabelCnt++;

    puts2(CodeHeaderFooter[0]);

    GenNumLabel(StructCpyLabel);

    puts2("  or r0 r6 r2\n"
          "  or r0 r6 r3");
    GenNumLabel(lbl);
    puts2("  read 0 r5 r6\n"
          "  add r5 1 r5\n"
          "  sub r4 1 r4\n"
          "  write 0 r3 r6\n"
          "  add r3 1 r3");
    
    printf2("  beq r4 r0 2\n");
    printf2("  jump ");GenPrintNumLabel(lbl);

    puts2("");
    puts2(" jumpr 0 r15");


    puts2(CodeHeaderFooter[1]);
  }

  // This is the place where we put the end of the wrapper code
  //  around the generated code

  printf2(
      ".code\n"
      "; Interrupt handlers\n"
      "; Has some administration before jumping to interrupt function\n"
      "; To prevent interfering with other stacks, they have their own stack\n"
      "; Also, all registers have to be backed up and restored to hardware stack\n"
      "; A return function has to be put on the stack as wel that the C code interrupt handler\n"
      "; will jump to when it is done\n"
      "\n"
      "Int:\n"
      "  push r1\n"
      "  push r2\n"
      "  push r3\n"
      "  push r4\n"
      "  push r5\n"
      "  push r6\n"
      "  push r7\n"
      "  push r8\n"
      "  push r9\n"
      "  push r10\n"
      "  push r11\n"
      "  push r12\n"
      "  push r13\n"
      "  push r14\n"
      "  push r15\n"
      "\n"
      "  load32 0x7FFFFF r13     ; initialize (BDOS) int stack address\n"
      "  load32 0 r14            ; initialize base pointer address\n"
      "  addr2reg Return_Interrupt r1 ; get address of return function\n"
      "  or r0 r1 r15            ; copy return addr to r15\n"
      "  jump Label_interrupt    ; jump to interrupt handler of C program\n"
      "                            ; should return to the address we just put on the stack\n"
      "  halt                    ; should not get here\n"
      "\n"
      "\n"
      "; Function that is called after the interrupt handler from C has returned\n"
      "; Restores all registers and issues RETI instruction to continue from original code\n"
      "Return_Interrupt:\n"
      "  pop r15\n"
      "  pop r14\n"
      "  pop r13\n"
      "  pop r12\n"
      "  pop r11\n"
      "  pop r10\n"
      "  pop r9\n"
      "  pop r8\n"
      "  pop r7\n"
      "  pop r6\n"
      "  pop r5\n"
      "  pop r4\n"
      "  pop r3\n"
      "  pop r2\n"
      "  pop r1\n"
      "\n"
      "  reti        ; return from interrrupt\n"
      "  halt        ; should not get here\n");
}

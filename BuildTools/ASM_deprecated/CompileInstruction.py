from bitstring import Bits

"""
Library for compiling single instructions
"""


# converts string to int
# string can by binary, decimal or hex
def getNumber(word, allowNeg=True, returnABS=False):
    value = 0
    # check for binary number
    if len(word) > 2 and word[:2] == "0b":
        try:
            value = int(word, 2)
        except ValueError:
            raise ValueError(str(word) + " is not a valid binary number")

    # check for hex number
    elif len(word) > 2 and word[:2] == "0x":
        try:
            value = int(word, 16)
        except ValueError:
            raise ValueError(str(word) + " is not a valid hex number")

    # check for decimal number
    else:
        try:
            value = int(word, 10)
        except ValueError:
            raise ValueError(str(word) + " is not a valid decimal number")

    # check for negative numbers
    if value < 0 and not allowNeg:
        raise ValueError(str(word) + " is a negative number (which are not allowed)")

    if allowNeg and returnABS:
        if value < 0:
            return abs(value), True
        else:
            return value, False
    return value


# converts string to int, representing the register
# string must be in format: rx
# where x is a number between 0 and 15 (including 0 and 15)
def getReg(word):
    value = 0

    # check if first char starts with an r
    if word[0].lower() == "r":
        # check for rbp and rsp (rbp == r14, rsp == r15)
        if word.lower() == "rbp":
            return 14
        if word.lower() == "rsp":
            return 15

        # parse number after r
        try:
            value = int(word[1:], 10)
        except ValueError:
            raise ValueError("Register" + str(word) + " is not a valid register")
    else:
        raise ValueError("Register " + str(word) + " does not start with 'r'")

    if value < 0 or value > 15:
        raise ValueError("Register " + str(word) + " is not a valid register")

    return value


# checks if the given signed value fits in the given number of bits
def CheckFitsInBits(value, bits, signed=True):
    if signed:  # signed numbers have one bit less
        bits -= 1
    if not (value.bit_length() <= bits):
        raise ValueError(
            "Value " + str(value) + " does not fit in " + str(bits) + " bits"
        )


"""
------------------LINE COMPILING FUNCTIONS---------------------
"""


# compiles halt instruction
# should have 0 arguments
def compileHalt(line):
    if len(line) != 1:
        raise Exception(
            "Incorrect number of arguments. Expected 0, but got " + str(len(line) - 1)
        )

    return "11111111111111111111111111111111 //Halt"


# compiles READ instruction
# should have 3 arguments
# arg1 should be a signed number that fits in 16 bits
# arg2 should be a valid register
# arg3 should be a valid register
def compileRead(line):
    if len(line) != 4:
        raise Exception(
            "Incorrect number of arguments. Expected 3, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1])

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16)
    const16 = format(arg1Int & 0xFFFF, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    areg = format(arg2Int, "04b")

    # convert arg3 to number
    arg3Int = getReg(line[3])

    # convert arg3 to binary
    dreg = format(arg3Int, "04b")

    # create instruction
    instruction = (
        "1110"
        + const16
        + areg
        + "0000"
        + dreg
        + " //Read at address in "
        + line[2]
        + " with offset "
        + line[1]
        + " to "
        + line[3]
    )

    return instruction


# compiles WRITE instruction
# should have 3 arguments
# arg1 should be a signed number that fits in 16 bits
# arg2 should be a valid register
# arg3 should be a valid register
def compileWrite(line):
    if len(line) != 4:
        raise Exception(
            "Incorrect number of arguments. Expected 3, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1])

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16)
    const16 = format(arg1Int & 0xFFFF, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    areg = format(arg2Int, "04b")

    # convert arg3 to number
    arg3Int = getReg(line[3])

    # convert arg3 to binary
    breg = format(arg3Int, "04b")

    # create instruction
    instruction = (
        "1101"
        + const16
        + areg
        + breg
        + "0000"
        + " //Write value in "
        + line[3]
        + " to address in "
        + line[2]
        + " with offset "
        + line[1]
    )

    return instruction


# compiles INTID instruction
# should have 1 argument
def compileIntID(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    dreg = format(arg1Int, "04b")

    # create instruction
    instruction = (
        "1100000000000000000000000000" + dreg + " //Save Interrupt ID to " + line[1]
    )

    return instruction


# compiles push instruction
# should have 1 argument
def compilePush(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    breg = format(arg1Int, "04b")

    # create instruction
    instruction = (
        "101100000000000000000000" + breg + "0000" + " //Push " + line[1] + " to stack"
    )

    return instruction


# compiles push instruction
# should have 1 argument
def compilePop(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    dreg = format(arg1Int, "04b")

    # create instruction
    instruction = (
        "1010000000000000000000000000" + dreg + " //Pop from stack to " + line[1]
    )

    return instruction


# compiles jump instruction
# should have 1 argument or a label
# arg1 should be a positive number that is within 27 bits unsigned
def compileJump(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    instruction = ""
    ARG1isAlabel = False

    try:
        getNumber(line[1], False)
        ARG1isAlabel = False
    except:
        ARG1isAlabel = True

    # if no label is given
    if not ARG1isAlabel:
        # convert arg1 to number
        arg1Int = getNumber(line[1], False)

        # convert arg1 to binary
        CheckFitsInBits(arg1Int, 27, False)
        const27 = format(arg1Int, "027b")

        # create instruction
        instruction = "1001" + const27 + "0" + " //Jump to constant address " + line[1]

    # if a label is given, process it later
    else:
        instruction = " ".join(line)

    return instruction


# compiles jump instruction to offset
# should have 1 argument
# arg1 should be a positive number that is within 27 bits unsigned
def compileJumpo(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    instruction = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1], False)

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 27, False)
    const27 = format(arg1Int, "027b")

    # create instruction
    instruction = "1001" + const27 + "1" + " //Jump to offset address " + line[1]

    return instruction


# compiles jump instruction to reg with offset
# should have 2 arguments
# arg1 should be a positive number that is within 16 bits signed
# arg2 should be a reg
def compileJumpr(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    instruction = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1])

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16)
    const16 = format(arg1Int & 0xFFF, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    breg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "1000"
        + const16
        + "0000"
        + breg
        + "000"
        + "0"
        + " //Jump to reg "
        + line[2]
        + " with offset "
        + line[1]
    )

    return instruction


# compiles jump instruction to offset in reg with offset
# should have 2 arguments
# arg1 should be a positive number that is within 16 bits signed
# arg2 should be a reg
def compileJumpro(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    instruction = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1])

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16)
    const16 = format(arg1Int & 0xFFFF, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    breg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "1000"
        + const16
        + "0000"
        + breg
        + "000"
        + "1"
        + " //Jump to offset in reg "
        + line[2]
        + " with offset "
        + line[1]
    )

    return instruction


# compiles branch instructions
# should have 3 arguments
# arg1 should be a valid register
# arg2 should be a valid register
# arg3 should be a positive number that is within 16 bits signed
def compileBranch(line, opcode, signed):
    if len(line) != 4:
        raise Exception(
            "Incorrect number of arguments. Expected 3, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    areg = format(arg1Int, "04b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    breg = format(arg2Int, "04b")

    arg3Int = 0

    # convert arg3 to number
    arg3Int = getNumber(line[3])

    # convert arg3 to binary
    CheckFitsInBits(arg3Int, 16)
    const16 = format(arg3Int & 0xFFFF, "016b")

    # signed bit
    signedBit = "0"
    signedString = "(unsigned)"
    if signed:
        signedBit = "1"
        signedString = "(signed)"

    # opcode to operation map
    branchOpcodeDict = {
        "000": " == ",
        "001": " > ",
        "010": " >= ",
        "011": " ?? ",
        "100": " != ",
        "101": " < ",
        "110": " <= ",
        "111": " ?? ",
    }

    # create instruction
    instruction = (
        "0110"
        + const16
        + areg
        + breg
        + opcode
        + signedBit
        + " //"
        + signedString
        + " If "
        + line[1]
        + branchOpcodeDict[opcode]
        + line[2]
        + ", then jump to offset "
        + line[3]
    )

    return instruction


# compiles BEQ instruction
def compileBEQ(line):
    return compileBranch(line, "000", False)


# compiles BGT instruction
def compileBGT(line):
    return compileBranch(line, "001", False)


# compiles BGTS instruction
def compileBGTS(line):
    return compileBranch(line, "001", True)


# compiles BGE instruction
def compileBGE(line):
    return compileBranch(line, "010", False)


# compiles BGES instruction
def compileBGES(line):
    return compileBranch(line, "010", True)


# compiles BNE instruction
def compileBNE(line):
    return compileBranch(line, "100", False)


# compiles BLT instruction
def compileBLT(line):
    return compileBranch(line, "101", False)


# compiles BLTS instruction
def compileBLTS(line):
    return compileBranch(line, "101", True)


# compiles BLE instruction
def compileBLE(line):
    return compileBranch(line, "110", False)


# compiles BLES instruction
def compileBLES(line):
    return compileBranch(line, "110", True)


# compiles SAVPC instruction
# should have 1 argument
def compileSavPC(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    dreg = format(arg1Int, "04b")

    # create instruction
    instruction = "0101000000000000000000000000" + dreg + " //Save PC to " + line[1]

    return instruction


# compiles RETI instruction
# should have 0 arguments
def compileReti(line):
    if len(line) != 1:
        raise Exception(
            "Incorrect number of arguments. Expected 0, but got " + str(len(line) - 1)
        )

    return "01000000000000000000000000000000 //Return from interrupt"


# compiles CCACHE instruction
# should have 0 arguments
def compileCcache(line):
    if len(line) != 1:
        raise Exception(
            "Incorrect number of arguments. Expected 0, but got " + str(len(line) - 1)
        )

    return "01110000000000000000000000000000 //Clear L1 Cache"


# compiles ARITH/ARITHC instructions, except LOAD/LOADHI
# should have 3 arguments
# arg1 should be a valid register
# arg2 should either be a number that is within 16 bits signed,
# or a valid register
# arg3 should be a valid register
def compileARITH(line, opcode):
    if len(line) != 4:
        raise Exception(
            "Incorrect number of arguments. Expected 3, but got " + str(len(line) - 1)
        )

    const16 = ""
    arg2Int = 0
    constantEnable = False
    breg = ""
    instruction = ""

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    areg = format(arg1Int, "04b")

    # convert arg2 to number
    if line[2][0].lower() == "r":  # if arg2 is a register argument
        constantEnable = False
        arg2Int = getReg(line[2])
    else:  # arg2 is a constant
        constantEnable = True
        arg2Int = getNumber(line[2])

    # convert arg2 to binary
    if constantEnable:
        CheckFitsInBits(arg2Int, 16)
        const16 = format(arg2Int & 0xFFFF, "016b")
    else:
        breg = format(arg2Int, "04b")

    # convert arg3 to number
    arg3Int = getReg(line[3])

    # convert arg3 to binary
    dreg = format(arg3Int, "04b")

    # opcode to operation map
    branchOpcodeDict = {
        "0000": " OR ",
        "0001": " AND ",
        "0010": " XOR ",
        "0011": " + ",
        "0100": " - ",
        "0101": " << ",
        "0110": " >> ",
        "0111": " ~A ",
        "1000": " Reserved ",
        "1001": " Reserved ",
        "1010": " SLT ",
        "1011": " SLTU ",
        "1110": " >> (signed) ",
        "1111": " Reserved ",
    }

    # create instruction
    if constantEnable:
        instruction = (
            "0001"
            + opcode
            + const16
            + areg
            + dreg
            + " //Compute "
            + line[1]
            + branchOpcodeDict[opcode]
            + line[2]
            + " and write result to "
            + line[3]
        )
    else:
        instruction = (
            "0000"
            + opcode
            + "000000000000"
            + areg
            + breg
            + dreg
            + " //Compute "
            + line[1]
            + branchOpcodeDict[opcode]
            + line[2]
            + " and write result to "
            + line[3]
        )

    return instruction


# compiles ARITHM/ARITHMC instructions
# should have 3 arguments
# arg1 should be a valid register
# arg2 should either be a number that is within 16 bits signed,
# or a valid register
# arg3 should be a valid register
def compileARITHM(line, opcode):
    if len(line) != 4:
        raise Exception(
            "Incorrect number of arguments. Expected 3, but got " + str(len(line) - 1)
        )

    const16 = ""
    arg2Int = 0
    constantEnable = False
    breg = ""
    instruction = ""

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg1 to binary
    areg = format(arg1Int, "04b")

    # convert arg2 to number
    if line[2][0].lower() == "r":  # if arg2 is a register argument
        constantEnable = False
        arg2Int = getReg(line[2])
    else:  # arg2 is a constant
        constantEnable = True
        arg2Int = getNumber(line[2])

    # convert arg2 to binary
    if constantEnable:
        CheckFitsInBits(arg2Int, 16)
        const16 = format(arg2Int & 0xFFFF, "016b")
    else:
        breg = format(arg2Int, "04b")

    # convert arg3 to number
    arg3Int = getReg(line[3])

    # convert arg3 to binary
    dreg = format(arg3Int, "04b")

    # opcode to operation map
    branchOpcodeDict = {
        "0000": " * (signed) ",
        "0001": " * (unsigned) ",
        "0010": " * (signed FP) ",
        "0011": " / (signed) ",
        "0100": " / (unsigned) ",
        "0101": " / (signed FP) ",
        "0110": " % (signed) ",
        "0111": " % (unsigned) ",
        "1000": " Reserved ",
        "1001": " Reserved ",
        "1010": " Reserved ",
        "1011": " Reserved ",
        "1110": " Reserved ",
        "1111": " Reserved ",
    }

    # create instruction
    if constantEnable:
        instruction = (
            "0011"
            + opcode
            + const16
            + areg
            + dreg
            + " //Compute "
            + line[1]
            + branchOpcodeDict[opcode]
            + line[2]
            + " and write result to "
            + line[3]
        )
    else:
        instruction = (
            "0010"
            + opcode
            + "000000000000"
            + areg
            + breg
            + dreg
            + " //Compute "
            + line[1]
            + branchOpcodeDict[opcode]
            + line[2]
            + " and write result to "
            + line[3]
        )

    return instruction


# compiles OR instruction
def compileOR(line):
    return compileARITH(line, "0000")


# compiles AND instruction
def compileAND(line):
    return compileARITH(line, "0001")


# compiles XOR instruction
def compileXOR(line):
    return compileARITH(line, "0010")


# compiles ADD instruction
def compileADD(line):
    return compileARITH(line, "0011")


# compiles SUB instruction
def compileSUB(line):
    return compileARITH(line, "0100")


# compiles SHIFTL instruction
def compileSHIFTL(line):
    return compileARITH(line, "0101")


# compiles SHIFTR instruction
def compileSHIFTR(line):
    return compileARITH(line, "0110")


# compiles SHIFTRS instruction
def compileSHIFTRS(line):
    return compileARITH(line, "1110")


# compiles SLT instruction
def compileSLT(line):
    return compileARITH(line, "1010")


# compiles SLTU instruction
def compileSLTU(line):
    return compileARITH(line, "1011")


# compiles MULTS instruction
def compileMULTS(line):
    return compileARITHM(line, "0000")


# compiles MULTU instruction
def compileMULTU(line):
    return compileARITHM(line, "0001")


# compiles MULTFP instruction
def compileMULTFP(line):
    return compileARITHM(line, "0010")


# compiles DIVS instruction
def compileDIVS(line):
    return compileARITHM(line, "0011")


# compiles DIVU instruction
def compileDIVU(line):
    return compileARITHM(line, "0100")


# compiles DIVFP instruction
def compileDIVFP(line):
    return compileARITHM(line, "0101")


# compiles MODS instruction
def compileMODS(line):
    return compileARITHM(line, "0110")


# compiles MODU instruction
def compileMODU(line):
    return compileARITHM(line, "0111")


# compiles NOT instruction
# should have 2 arguments
# arg1 should be a valid register
# arg2 should be a valid register
def compileNOT(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    instruction = ""

    # convert arg1 to number
    arg1Int = getReg(line[1])

    # convert arg2 to binary
    areg = format(arg1Int, "04b")

    # convert arg3 to number
    arg2Int = getReg(line[2])

    # convert arg3 to binary
    dreg = format(arg2Int, "04b")

    instruction = (
        "00000111000000000000"
        + areg
        + "0000"
        + dreg
        + " //Compute NOT "
        + line[1]
        + " and write result to "
        + line[2]
    )

    return instruction


# compiles load instruction
# should have 2 arguments
# arg1 should be a positive number that is within 16 bits unsigned
# arg2 should be a valid register
def compileLoad(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1], False)

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16, False)
    const16 = format(arg1Int, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    dreg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "00011100" + const16 + dreg + dreg + " //Set " + line[2] + " to " + line[1]
    )

    return instruction


# compiles loadhi instruction
# should have 2 arguments
# arg1 should be a positive number that is within 16 bits unsigned
# arg2 should be a valid register
def compileLoadHi(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1], False)

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16, False)
    const16 = format(arg1Int, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    dreg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "00011101"
        + const16
        + dreg
        + dreg
        + " //Set highest 16 bits of "
        + line[2]
        + " to "
        + line[1]
    )

    return instruction


# compiles addr2reg instruction
# should have 2 arguments
# arg 1 should be a valid label
# arg 2 should be a reg
def compileAddr2reg(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    instruction = "loadLabelHigh " + line[1] + " " + line[2]

    # check if register is valid
    getReg(line[2])

    return instruction


# compiles load32 instruction
# should have 2 arguments
# arg 1 should be a number within 32 bits
# arg 2 should be a reg
def compileLoad32(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    const32 = ""

    # convert arg1 to number
    arg1Int, neg = getNumber(line[1], True, True)
    if neg:
        arg1Int = -arg1Int  # because getNumber does ABS on it

    # check if it fits in 16 bits, so we can skip the loadhi
    if not neg:
        try:
            CheckFitsInBits(arg1Int, 16, False)
            return compileLoad(line)
        except:
            pass  # continue

    # check if it fits in 32 bits
    if not neg:
        CheckFitsInBits(arg1Int, 32, False)
    else:
        if arg1Int < -2147483648:
            raise ValueError(
                "Negative value " + str(arg1Int) + " does not fit in 32 bits (signed)"
            )

    # convert to 32 bit
    const32 = ""
    if not neg:
        const32 = format(arg1Int, "032b")
    else:
        b = Bits(int=arg1Int, length=32)  # signed binary notation
        const32 = b.bin

    constLow = int(const32[0:16], 2)
    constHigh = int(const32[16:32], 2)

    # create instruction
    instruction = "loadBoth " + str(constLow) + " " + str(constHigh) + " " + line[2]

    return instruction


# compiles compileLoadLabelLow instruction
def compileLoadLabelLow(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1], False)

    x = "{0:027b}".format(arg1Int)

    # x[:11] highest 11 bits
    # x[11:] lowest 16 bits

    arg1Int = getNumber("0b" + x[11:], False)

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16, False)  # should not be needed
    const16 = format(arg1Int, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    dreg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "00011100" + const16 + dreg + dreg + " //Set " + line[2] + " to " + str(arg1Int)
    )

    return instruction


# compiles compileLoadLabelHigh instruction
def compileLoadLabelHigh(line):
    if len(line) != 3:
        raise Exception(
            "Incorrect number of arguments. Expected 2, but got " + str(len(line) - 1)
        )

    const16 = ""

    # convert arg1 to number
    arg1Int = getNumber(line[1], False)

    x = "{0:027b}".format(arg1Int)

    # x[:11] highest 4 bits
    # x[11:] lowest 16 bits

    arg1Int = getNumber("0b" + x[:11], False)

    # convert arg1 to binary
    CheckFitsInBits(arg1Int, 16, False)
    const16 = format(arg1Int, "016b")

    # convert arg2 to number
    arg2Int = getReg(line[2])

    # convert arg2 to binary
    dreg = format(arg2Int, "04b")

    # create instruction
    instruction = (
        "00011101"
        + const16
        + dreg
        + dreg
        + " //Set highest 16 bits of "
        + line[2]
        + " to "
        + str(arg1Int)
    )

    return instruction


# compiles nop instruction
# should have 0 arguments
def compileNop(line):
    if len(line) != 1:
        raise Exception(
            "Incorrect number of arguments. Expected 0, but got " + str(len(line) - 1)
        )

    return "00000000000000000000000000000000 //NOP"


# compiles .dw instruction
def compileDw(line):
    if len(line) < 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1 or more, but got "
            + str(len(line) - 1)
        )

    instruction = "data"

    for i in line:
        if i != ".dw":
            number = getNumber(i, True)
            if i[0] == "-" and int(i, 10) < 0:
                CheckFitsInBits(number, 33, True)
            else:
                CheckFitsInBits(number, 32, False)
            instruction = instruction + " " + format(number & 0xFFFFFFFF, "032b")

    return instruction


# compiles .dd instruction
def compileDd(line):
    if len(line) < 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1 or more, but got "
            + str(len(line) - 1)
        )

    instruction = "data"

    counter = 1
    for i in line:
        if i != ".dd":
            number = getNumber(i, False)
            CheckFitsInBits(number, 16, False)
            if counter < 1:
                counter = counter + 1
                instruction = instruction + "" + "{0:016b}".format(number)
            else:
                instruction = instruction + " " + "{0:016b}".format(number)
                counter = 0

    if counter == 0:
        instruction = instruction + "0000000000000000"

    return instruction


# compiles .db instruction
def compileDb(line):
    if len(line) < 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1 or more, but got "
            + str(len(line) - 1)
        )

    instruction = "data"

    counter = 3
    for i in line:
        if i != ".db":
            number = getNumber(i, False)
            CheckFitsInBits(number, 8, False)
            if counter < 3:
                counter = counter + 1
                instruction = instruction + "" + "{0:08b}".format(number)
            else:
                instruction = instruction + " " + "{0:08b}".format(number)
                counter = 0

    if counter == 0:
        instruction = instruction + "000000000000000000000000"
    if counter == 1:
        instruction = instruction + "0000000000000000"
    if counter == 2:
        instruction = instruction + "00000000"

    return instruction


# compiles .ds instruction by converting it to a .db instruction
def compileDs(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    # .db instruction
    dbList = []

    dbList.append(".db")

    # check for " "
    if line[1][0] == '"' and line[1][-1] == '"':
        dbList = dbList + [str(ord(char)) for char in line[1][1:-1]]
        # TODO convert string to ascii list using [ord(char) for char in string]
    else:
        raise Exception("Invalid string: " + line[1])

    return compileDb(dbList)


# compiles .dl
# should have 1 argument or a label
# arg1 should be a positive number that is within 27 bits unsigned
def compileDl(line):
    if len(line) != 2:
        raise Exception(
            "Incorrect number of arguments. Expected 1, but got " + str(len(line) - 1)
        )

    instruction = ""
    ARG1isAlabel = False

    try:
        getNumber(line[1], False)
        ARG1isAlabel = False
    except:
        ARG1isAlabel = True

    # if no label is given
    if not ARG1isAlabel:
        # convert arg1 to number
        arg1Int = getNumber(line[1], False)

        # convert arg1 to binary
        CheckFitsInBits(arg1Int, 27, False)
        const27 = format(arg1Int, "027b")

        # create data (only the label address)
        instruction = "00000" + const27 + " //Label data " + line[1]

    # if a label is given, process it later
    else:
        instruction = " ".join(line)

    return instruction


# compile nothing
def compileNothing(line):
    return "ignore"

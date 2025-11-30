from enum import Enum
from dataclasses import dataclass
from typing import Self


class StringParsableEnum(Enum):
    """Base Enum class with a from_str method for easy string parsing."""

    @classmethod
    def from_str(cls, value: str) -> Self:
        """Convert a string to the corresponding Enum value using fast lookup."""
        _lookup = {e.value: e for e in cls.__members__.values()}
        if value in _lookup:
            return _lookup[value]
        raise ValueError(f"Could not parse into {cls.__name__}: {value}")


class ProgramType(StringParsableEnum):
    """Types of programs to assemble."""

    BDOS = "bdos"
    USER_BDOS = "userbdos"
    BARE_METAL = "baremetal"
    HEADERLESS = "headerless"


class DataInstructionType(StringParsableEnum):
    """Types of data instructions."""

    WORD = ".dw"

    # Merged to fit into 32-bit words
    BYTE_MERGED = ".dbb"
    DOUBLE_MERGED = ".ddb"
    STRING_MERGED = ".dsb"

    # Each data instruction has its own 32-bit word
    BYTE_SPACED = ".dbw"
    DOUBLE_SPACED = ".ddw"
    STRING_SPACED = ".dsw"


class DirectiveType(StringParsableEnum):
    """Types of assembly directives."""

    INITIALIZED_DATA = ".data"
    READ_ONLY_DATA = ".rdata"
    UNINITIALIZED_DATA = ".bss"
    CODE_DATA = ".code"


class ControlOperation(StringParsableEnum):
    """Types of control operations."""

    HALT = "halt"
    SAVE_PROGRAM_COUNTER = "savpc"
    CLEAR_CACHE = "ccache"
    NOP = "nop"
    ADDRESS_TO_REGISTER = "addr2reg"
    RETURN_INTERRUPT = "reti"
    READ_INTERRUPT_ID = "readintid"


class MemoryOperation(StringParsableEnum):
    """Types of memory operations."""

    READ = "read"
    WRITE = "write"
    PUSH = "push"
    POP = "pop"


class SingleCycleArithmeticOperation(StringParsableEnum):
    """Types of single cycle arithmetic operations."""

    ADD = "add"
    SUBTRACT = "sub"
    SET_LESS_THAN = "slt"
    SET_LESS_THAN_UNSIGNED = "sltu"
    LOAD_LOW = "load"
    LOAD_HIGH = "loadhi"
    LOAD_32_BIT = "load32"

    # Bitwise
    AND = "and"
    OR = "or"
    XOR = "xor"
    NOT = "not"
    SHIFT_LEFT = "shiftl"
    SHIFT_RIGHT = "shiftr"
    SHIFT_RIGHT_SIGNED = "shiftrs"


class MultiCycleArithmeticOperation(StringParsableEnum):
    """Types of multi cycle arithmetic operations."""

    MULTIPLY_SIGNED = "mults"
    MULTIPLY_UNSIGNED = "multu"
    MULTIPLY_FIXED_POINT = "multfp"
    DIVIDE_SIGNED = "divs"
    DIVIDE_UNSIGNED = "divu"
    DIVIDE_FIXED_POINT = "divfp"
    MODULO_SIGNED = "mods"
    MODULO_UNSIGNED = "modu"


class BranchOperation(StringParsableEnum):
    """Types of branch operations."""

    BRANCH_EQUAL = "beq"
    BRANCH_NOT_EQUAL = "bne"

    BRANCH_GREATER_UNSIGNED = "bgt"
    BRANCH_GREATER_EQUAL_UNSIGNED = "bge"
    BRANCH_LESS_UNSIGNED = "blt"
    BRANCH_LESS_EQUAL_UNSIGNED = "ble"

    BRANCH_GREATER_SIGNED = "bgts"
    BRANCH_GREATER_EQUAL_SIGNED = "bges"
    BRANCH_LESS_SIGNED = "blts"
    BRANCH_LESS_EQUAL_SIGNED = "bles"


class JumpOperation(StringParsableEnum):
    """Types of jump operations."""

    JUMP = "jump"
    JUMP_OFFSET = "jumpo"
    JUMP_REGISTER = "jumpr"
    JUMP_REGISTER_OFFSET = "jumpro"


class InstructionOpcode(Enum):
    """Enum class to represent instruction opcodes."""

    HALT = "1111"
    READ = "1110"
    WRITE = "1101"
    INTID = "1100"
    PUSH = "1011"
    POP = "1010"
    JUMP = "1001"
    JUMPR = "1000"
    CCACHE = "0111"
    BRANCH = "0110"
    SAVPC = "0101"
    RETI = "0100"
    ARITHM = "0011"
    ARITHMC = "0010"
    ARITHC = "0001"
    ARITH = "0000"


class BranchOpcode(Enum):
    """Enum class to represent branch opcodes."""

    BEQ = "000"
    BGT = "001"
    BGE = "010"
    # Reserved
    BNE = "100"
    BLT = "101"
    BLE = "110"


class ArithOpcode(Enum):
    """Enum class to represent single cycle arithmetic opcodes."""

    OR = "0000"
    AND = "0001"
    XOR = "0010"
    ADD = "0011"
    SUB = "0100"
    SHIFTL = "0101"
    SHIFTR = "0110"
    NOTA = "0111"
    # Reserved
    # Reserved
    SLT = "1010"
    SLTU = "1011"
    LOAD = "1100"
    LOADHI = "1101"
    SHIFTRS = "1110"
    # Reserved


class ArithmOpcode(Enum):
    """Enum class to represent multi cycle arithmetic opcodes."""

    MULTS = "0000"
    MULTU = "0001"
    MULTFP = "0010"
    DIVS = "0011"
    DIVU = "0100"
    DIVFP = "0101"
    MODS = "0110"
    MODU = "0111"


class RegisterValue(StringParsableEnum):
    """Types of registers."""

    R0 = "r0"
    R1 = "r1"
    R2 = "r2"
    R3 = "r3"
    R4 = "r4"
    R5 = "r5"
    R6 = "r6"
    R7 = "r7"
    R8 = "r8"
    R9 = "r9"
    R10 = "r10"
    R11 = "r11"
    R12 = "r12"
    R13 = "r13"
    R14 = "r14"
    R15 = "r15"


class Register:
    """Class to represent a register in assembly code."""

    def __init__(self, register: RegisterValue) -> None:
        self.register = register

    def __str__(self):
        return self.register.value

    def to_binary(self) -> str:
        return f"{int(self.register.value[1:]):04b}"

    @staticmethod
    def from_str(register_str: str) -> "Register":
        """Parse a register from a string."""
        return Register(RegisterValue.from_str(register_str))

    def __repr__(self) -> str:
        return f"Register {self.register}"


class Label:
    """Class to represent a label in assembly code."""

    def __init__(self, label: str, target_address: int | None = None) -> None:
        self.label = label
        self.target_address = target_address

    def __str__(self):
        return self.label

    def __repr__(self) -> str:
        target_address_str = (
            f" -> {self.target_address}" if self.target_address else " -> ?"
        )
        return f"Label {self.label}{target_address_str}"

    def __eq__(self, value: object) -> bool:
        if not isinstance(value, Label):
            return NotImplemented
        return self.label == value.label

    def __hash__(self) -> int:
        return hash(self.label)


class Number:
    """Class to represent a number, while keeping its original string representation in binary, hexadecimal or decimal format."""

    value: int
    original: str | None

    def __init__(self, value: int | str, original: str | None = None) -> None:
        if isinstance(value, str):
            # Parse from string
            parsed = self._from_str(value)
            self.value = parsed.value
            self.original = parsed.original
        else:
            # Use provided int and original
            self.value = value
            self.original = original

    @staticmethod
    def _from_str(input_str: str) -> "Number":
        """Parse number from a string representing a binary, hexadecimal or decimal number."""
        try:
            if input_str.startswith(("0b", "0B")):
                return Number(value=int(input_str, 2), original=input_str)
            elif input_str.startswith(("0x", "0X")):
                return Number(value=int(input_str, 16), original=input_str)
            else:
                return Number(value=int(input_str), original=input_str)
        except ValueError:
            raise ValueError(f"Invalid number: {input_str}")

    def to_binary(self, bits: int) -> str:
        """Convert the number to a binary string with a fixed number of bits."""
        # Treat numbers as signed two's complement for range checking
        signed_min = -(1 << (bits - 1))
        signed_max = (1 << (bits - 1)) - 1
        if not (signed_min <= self.value <= signed_max):
            raise ValueError(f"Number must fit (signed) in {bits} bits")
        return f"{self.value & ((1 << bits) - 1):0{bits}b}"

    def __int__(self) -> int:
        return self.value

    def __str__(self) -> str:
        return str(self.value)

    def __repr__(self) -> str:
        return f"Number {self.original} -> {self.value}"


@dataclass
class SourceLine:
    """Class to represent a line of source code."""

    line: str
    source_line_number: int
    source_file_name: str

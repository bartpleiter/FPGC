from enum import Enum
from dataclasses import dataclass


class StringParsableEnum(Enum):
    """Base Enum class with a from_str method for easy string parsing."""

    @classmethod
    def from_str(cls, value: str):
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
    SAVE_PROGRAM_COUNTER = "savepc"
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


class Register(StringParsableEnum):
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

    RSP = "rsp"
    RBP = "rbp"


class Label:
    """Class to represent a label in assembly code."""

    def __init__(self, label: str, target_address: int | None = None) -> None:
        self.label = label
        self.target_address = target_address

    def __repr__(self) -> str:
        target_address_str = (
            f" -> {self.target_address}" if self.target_address else " -> ?"
        )
        return f"Label {self.label}{target_address_str}"


class Number:
    """Class to represent a number in binary, hexadecimal or decimal format."""

    def __init__(self, input_str: str) -> None:
        self.original = input_str
        self.value = self._parse_number(self.original)

    def _parse_number(self, input_str: str) -> int:
        """Parse number from a string representing a binary, hexadecimal or decimal number.
        TODO: Convert to a from_str method for consistency with the Enums."""
        try:
            if input_str.startswith(("0b", "0B")):
                return int(input_str, 2)
            elif input_str.startswith(("0x", "0X")):
                return int(input_str, 16)
            else:
                return int(input_str)
        except ValueError:
            raise ValueError(f"Invalid number: {input_str}")

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

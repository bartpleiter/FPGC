from enum import Enum
from dataclasses import dataclass


class ProgramType(Enum):
    """Types of programs to assemble."""

    BDOS = "bdos"
    USER_BDOS = "userbdos"
    BARE_METAL = "baremetal"
    HEADERLESS = "headerless"


class DataInstructionType(Enum):
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


class DirectiveType(Enum):
    """Types of assembly directives."""

    INITIALIZED_DATA = ".data"
    READ_ONLY_DATA = ".rdata"
    UNINITIALIZED_DATA = ".bss"
    CODE_DATA = ".code"


class Number:
    """Class to represent a number in binary, hexadecimal or decimal format"""

    def __init__(self, input_str: str) -> None:
        self.original = input_str
        self.value = self._parse_number(self.original)

    def _parse_number(self, input_str: str) -> int:
        """Parse number from a string representing a binary, hexadecimal or decimal number"""
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


@dataclass
class SourceLine:
    """Class to represent a line of source code."""

    line: str
    source_line_number: int
    source_file_name: str

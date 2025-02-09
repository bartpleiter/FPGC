from abc import ABC, abstractmethod
from enum import Enum
import logging

from asmpy.utils import read_input_file


class ProgramType(Enum):
    """Types of programs to assemble"""

    BDOS = "bdos"
    USER_BDOS = "userbdos"
    BARE_METAL = "baremetal"
    HEADERLESS = "headerless"


class DataInstructionType(Enum):
    """Types of data instructions"""

    WORD = ".dw"

    # Merged to fit into 32-bit words
    BYTE_MERGED = ".dbb"
    DOUBLE_MERGED = ".ddb"
    STRING_MERGED = ".dsb"

    # Each data instruction has its own 32-bit word
    BYTE_SPACED = ".dbw"
    DOUBLE_SPACED = ".ddw"
    STRING_SPACED = ".dsw"


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


class Define:
    """Class to represent a define directive."""

    def __init__(self, name: str, value: str) -> None:
        self.name = name
        self.value = value


class Include:
    """Class to represent an include directive."""

    def __init__(self, file: str) -> None:
        self.file = file

    def get_contents(self) -> list[str]:
        """Get the contents of the included file."""
        return read_input_file(self.file)


class PreprocessorDirective(ABC):
    """Base class to represent a preprocessor directive."""

    def __init__(self, code_str: str, comment: str, original: str = ""):
        self.code_str = code_str
        self.comment = comment
        self.original = original

        self._logger = logging.getLogger()

        self._parse_code()

    @staticmethod
    def parse_line(line: str) -> "PreprocessorDirective":
        """Parse the line into an appropriate PreprocessorDirective subclass."""
        original = line
        parts = original.split(";")
        code_str = parts[0].strip()
        comment = parts[1].strip() if len(parts) > 1 else ""

        if code_str.lower().startswith("#include"):
            return IncludePreprocessorDirective(code_str, comment, original)
        elif code_str.lower().startswith("#define"):
            return DefinePreprocessorDirective(code_str, comment, original)
        else:
            raise ValueError(f"Invalid preprocessor directive: {code_str}")

    @abstractmethod
    def _parse_code(self):
        pass

    @abstractmethod
    def __repr__(self):
        pass


class IncludePreprocessorDirective(PreprocessorDirective):
    """Class to represent an include preprocessor directive"""

    def _parse_code(self):
        code_parts = self.code_str.split()
        if len(code_parts) != 2:
            raise ValueError(f"Invalid include directive: {self.code_str}")

        self.include_file = Include(code_parts[1])

    def __repr__(self):
        return f"IncludePreprocessorDirective({self.code_str}, {self.comment})"


class DefinePreprocessorDirective(PreprocessorDirective):
    """Class to represent a define preprocessor directive"""

    def _parse_code(self):
        code_parts = self.code_str.split()
        if len(code_parts) != 3:
            raise ValueError(f"Invalid define directive: {self.code_str}")

        self.define = Define(code_parts[1], code_parts[2])

    def __repr__(self):
        return f"DefinePreprocessorDirective({self.code_str}, {self.comment})"


class AssemblyLine(ABC):
    """Base class to represent a single line of assembly code"""

    def __init__(self, code_str: str, comment: str, original: str = ""):
        self.code_str = code_str
        self.comment = comment
        self.original = original

        self._logger = logging.getLogger()

    @staticmethod
    def _matches_data_instruction_type(x: str) -> bool:
        for instruction in DataInstructionType:
            if x.startswith(instruction.value):
                return True
        return False

    @staticmethod
    def parse_line(line: str) -> "AssemblyLine":
        """Parse the line into an appropriate AssemblyLine subclass."""
        original = line
        parts = original.split(";")
        code_str = parts[0].strip()
        comment = parts[1].strip() if len(parts) > 1 else ""

        if not code_str:
            return CommentAssemblyLine(code_str, comment, original)
        elif AssemblyLine._matches_data_instruction_type(code_str):
            return DataAssemblyLine(code_str, comment, original)
        elif code_str.startswith("."):
            return DirectiveAssemblyLine(code_str, comment, original)
        elif code_str.endswith(":"):
            return LabelAssemblyLine(code_str, comment, original)
        else:
            return InstructionAssemblyLine(code_str, comment, original)

    @abstractmethod
    def _parse_code(self):
        """Parse the code string."""
        pass

    @abstractmethod
    def __repr__(self):
        pass


class DirectiveAssemblyLine(AssemblyLine):
    """Class to represent a directive line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"DirectiveAssemblyLine({self.code_str}, {self.comment})"


class LabelAssemblyLine(AssemblyLine):
    """Class to represent a label line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"LabelAssemblyLine({self.code_str}, {self.comment})"


class InstructionAssemblyLine(AssemblyLine):
    """Class to represent an instruction line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"InstructionAssemblyLine({self.code_str}, {self.comment})"


class DataAssemblyLine(AssemblyLine):
    """Class to represent a data line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"DataAssemblyLine({self.code_str}, {self.comment})"


class CommentAssemblyLine(AssemblyLine):
    """Class to represent a comment line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"CommentAssemblyLine({self.comment})"

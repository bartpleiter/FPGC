from abc import ABC, abstractmethod
import logging

from asmpy.models.data_types import DataInstructionType, DirectiveType, Number, SourceLine


class AssemblyLine(ABC):
    """Base class to represent a single line of assembly code"""

    directive: DirectiveType

    def __init__(
        self,
        code_str: str,
        comment: str,
        original: str = "",
        source_line_number: int = 0,
        source_file_name: str = "",
    ) -> None:
        self.code_str = code_str
        self.comment = comment
        self.original = original
        self.source_line_number = source_line_number
        self.source_file_name = source_file_name

        self._parse_code()

        self._logger = logging.getLogger()

    @staticmethod
    def _matches_data_instruction_type(x: str) -> bool:
        x = x.split()[0]
        for instruction in DataInstructionType:
            if x ==instruction.value:
                return True
        return False

    @staticmethod
    def parse_line(source_line: SourceLine) -> "AssemblyLine":
        """Parse the line into an appropriate AssemblyLine subclass."""
        original = source_line.line
        parts = original.split(";")
        code_str = parts[0].strip()
        comment = parts[1].strip() if len(parts) > 1 else ""

        if not code_str:
            return CommentAssemblyLine(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
        elif AssemblyLine._matches_data_instruction_type(code_str):
            return DataAssemblyLine(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
        elif code_str.startswith("."):
            return DirectiveAssemblyLine(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
        elif code_str.endswith(":"):
            return LabelAssemblyLine(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
        else:
            return InstructionAssemblyLine(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )

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
        parts = self.code_str.split()
        if len(parts) != 1:
            raise ValueError(f"Invalid directive: {self.code_str}")
        
        self.directive = DirectiveType(parts[0])

    def __repr__(self):
        return f"{self.source_file_name}:{self.source_line_number} -> {self.directive}"


class LabelAssemblyLine(AssemblyLine):
    """Class to represent a label line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"{self.source_file_name}:{self.source_line_number} -> Label {self.code_str} # {self.comment}"


class InstructionAssemblyLine(AssemblyLine):
    """Class to represent an instruction line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"{self.source_file_name}:{self.source_line_number} -> Instruction {self.code_str} # {self.comment}"


class DataAssemblyLine(AssemblyLine):
    """Class to represent a data line of assembly code"""

    def _parse_code(self):
        self.data_instruction_values = []
        data_instruction_type_str = self.code_str.split()[0]

        for instruction in DataInstructionType:
            if data_instruction_type_str ==instruction.value:
                self.data_instruction_type = instruction
                break
        
        if not self.data_instruction_type:
            raise ValueError(f"Invalid data instruction: {self.code_str}")
        
        data_instruction_values = self.code_str.split()[1:]
        for value in data_instruction_values:
            self.data_instruction_values.append(Number(value))


    def __repr__(self):
        return f"{self.source_file_name}:{self.source_line_number} -> {self.data_instruction_type} {self.data_instruction_values}"


class CommentAssemblyLine(AssemblyLine):
    """Class to represent a comment line of assembly code"""

    def _parse_code(self):
        pass

    def __repr__(self):
        return f"{self.source_file_name}:{self.source_line_number} -> # {self.comment}"

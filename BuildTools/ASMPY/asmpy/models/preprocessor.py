from abc import ABC, abstractmethod
import logging
from asmpy.models.data_types import SourceLine


class Define:
    """Class to represent a define directive."""

    def __init__(self, name: str, value: str) -> None:
        self.name = name
        self.value = value


class Include:
    """Class to represent an include directive."""

    def __init__(self, file: str) -> None:
        self.file = file.strip('"')

    @property
    def file_name(self) -> str:
        return self.file


class PreprocessorDirective(ABC):
    """Base class to represent a preprocessor directive."""

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

        self._logger = logging.getLogger()

        self._parse_code()

    @staticmethod
    def parse_line(source_line: SourceLine) -> "PreprocessorDirective":
        """Parse the line into an appropriate PreprocessorDirective subclass."""
        original = source_line.line
        parts = original.split(";")
        code_str = parts[0].strip()
        comment = parts[1].strip() if len(parts) > 1 else ""

        if code_str.lower().startswith("#include"):
            return IncludePreprocessorDirective(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
        elif code_str.lower().startswith("#define"):
            return DefinePreprocessorDirective(
                code_str,
                comment,
                original,
                source_line.source_line_number,
                source_line.source_file_name,
            )
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

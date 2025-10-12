from pathlib import Path
from asmpy.utils import read_input_file
from asmpy.models.preprocessor import (
    DefinePreprocessorDirective,
    IncludePreprocessorDirective,
    PreprocessorDirective,
)
from asmpy.models.data_types import SourceLine


class Preprocessor:
    """Class to preprocess an assembly file.
    Allows for recursive preprocessing of include files."""

    def __init__(
        self,
        source_input_lines: list[SourceLine],
        file_path: Path,
        already_included_filenames: list[str] = [],
    ) -> None:
        self.source_input_lines = source_input_lines
        self.file_path = file_path
        self.already_included_filenames = already_included_filenames
        self.current_directory = file_path.parent

        self.preprocessed_lines = []

    @staticmethod
    def _parse_preprocessor_directives(
        input_lines: list[SourceLine],
    ) -> list[PreprocessorDirective]:
        """Parse preprocessor directives from input lines."""
        return [
            PreprocessorDirective.parse_line(source_line)
            for source_line in input_lines
            if source_line.line.startswith("#")
        ]

    def preprocess(self) -> list[SourceLine]:
        """Preprocess the input file."""

        if self.file_path.name in self.already_included_filenames:
            return []

        self.already_included_filenames.append(self.file_path.name)

        preprocessor_directives = self._parse_preprocessor_directives(
            self.source_input_lines
        )
        self.preprocessed_lines = self.source_input_lines
        for directive in preprocessor_directives:
            if isinstance(directive, IncludePreprocessorDirective):
                include_contents = read_input_file(
                    self.current_directory / directive.include_file.file_name
                )
                include_preprocessed = Preprocessor(
                    source_input_lines=include_contents,
                    file_path=self.current_directory / directive.include_file.file_name,
                    already_included_filenames=self.already_included_filenames,
                ).preprocess()
                # Append the include file contents to the end of the current file
                self.preprocessed_lines = self.preprocessed_lines + include_preprocessed

            elif isinstance(directive, DefinePreprocessorDirective):
                for source_line in self.preprocessed_lines:
                    source_line.line = source_line.line.replace(
                        directive.define.name, directive.define.value
                    )

        return self.preprocessed_lines

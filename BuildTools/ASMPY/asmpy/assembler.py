"""
Steps needed for assembling:
1. Read the input file
2. Read input lines while parsing them into objects (keep list of DEFINEs)
3. Process library import objects by reading them and inserting them into the assembly ordered list
4. Reorder lines based on section
5. Remove unreachable lines
6. Do pass one of the assembler, after which the number of lines should match the number of words in the output
7. Do pass two of the assembler to replace labels with their addresses
8. Verify output
9. Write output to file
"""

import logging
from asmpy.utils import read_input_file
from asmpy.models import (
    AssemblyLine,
    DefinePreprocessorDirective,
    IncludePreprocessorDirective,
    ProgramType,
    Number,
    PreprocessorDirective,
)


class Assembler:
    def __init__(
        self,
        input_file_path: str,
        output_file_path: str,
        offset_address: Number = Number("0"),
        program_type: ProgramType = ProgramType.BARE_METAL,
    ) -> None:
        self.input_file_path = input_file_path
        self.output_file_path = output_file_path
        self.offset_address = offset_address
        self.program_type = program_type

        self._logger = logging.getLogger()

        self._assembly_lines: list = []

    @staticmethod
    def _parse_preprocessor_directives(
        input_lines: list[str],
    ) -> list[PreprocessorDirective]:
        """Parse preprocessor directives from input lines."""
        return [
            PreprocessorDirective.parse_line(line)
            for line in input_lines
            if line.startswith("#")
        ]

    @staticmethod
    def _parse_input_lines(input_lines: list[str]) -> list[AssemblyLine]:
        """Parse input lines into a list of objects while discarding empty lines and preprocessor directives."""
        return [
            AssemblyLine.parse_line(line)
            for line in input_lines
            if line.strip() and not line.strip().startswith("#")
        ]

    def preprocess(self) -> None:
        """Preprocess the input file."""

        main_input_lines = read_input_file(self.input_file_path)
        preproccoressor_directives = self._parse_preprocessor_directives(
            main_input_lines
        )
        self.preprocessed_lines = main_input_lines
        for directive in preproccoressor_directives:
            if isinstance(directive, IncludePreprocessorDirective):
                include_contents = directive.include_file.get_contents()
                self.preprocessed_lines = self.preprocessed_lines.append(
                    include_contents
                )
            elif isinstance(directive, DefinePreprocessorDirective):
                self.preprocessed_lines = [
                    line.replace(directive.define.name, directive.define.value)
                    for line in self.preprocessed_lines
                ]

    def assemble(self) -> None:
        """Assemble the preprocessed input file."""

        if not self.preprocessed_lines:
            raise ValueError("Preprocessing must be done before assembling.")

        self.parsed_lines = self._parse_input_lines(self.preprocessed_lines)

        for line in self.parsed_lines:
            self._logger.debug(line)

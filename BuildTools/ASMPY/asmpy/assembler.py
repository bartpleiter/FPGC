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
from asmpy.models.assembly_line import (
    AssemblyLine,
)
from asmpy.models.data_types import (
    ProgramType,
    Number,
    SourceLine,
)


class Assembler:
    def __init__(
        self,
        preprocessed_input_lines: list[SourceLine],
        output_file_path: str,
        offset_address: Number = Number("0"),
        program_type: ProgramType = ProgramType.BARE_METAL,
    ) -> None:
        self.preprocessed_input_lines = preprocessed_input_lines
        self.output_file_path = output_file_path
        self.offset_address = offset_address
        self.program_type = program_type

        self._logger = logging.getLogger()

        self._assembly_lines: list = []

    @staticmethod
    def _parse_input_lines(input_lines: list[SourceLine]) -> list[AssemblyLine]:
        """Parse input lines into a list of objects while discarding empty lines and preprocessor directives."""
        return [
            AssemblyLine.parse_line(source_line)
            for source_line in input_lines
            if source_line.line.strip() and not source_line.line.strip().startswith("#")
        ]

    def assemble(self) -> None:
        """Assemble the preprocessed input file."""

        self.parsed_lines = self._parse_input_lines(self.preprocessed_input_lines)

        for line in self.parsed_lines:
            self._logger.debug(line)

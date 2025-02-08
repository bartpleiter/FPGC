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
from asmpy.models import ProgramType, Number


class Assembler:
    def __init__(
        self,
        input_file_path: str,
        output_file_path: str,
        offset_address: Number = Number("0"),
        program_type: ProgramType = ProgramType.BareMetal,
    ) -> None:
        self.input_file_path = input_file_path
        self.output_file_path = output_file_path
        self.offset_address = offset_address
        self.program_type = program_type

        self._logger = logging.getLogger()

    def assemble(self) -> None:
        pass

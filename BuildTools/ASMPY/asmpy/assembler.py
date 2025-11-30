import logging
from asmpy.models.assembly_line import (
    AssemblyLine,
    CommentAssemblyLine,
    DirectiveAssemblyLine,
    InstructionAssemblyLine,
    LabelAssemblyLine,
)
from asmpy.models.data_types import (
    Label,
    ProgramType,
    Number,
    SourceLine,
    DirectiveType,
    BranchOperation,
)


class Assembler:
    # Order of assembly directives in the output
    DIRECTIVE_ORDER = [
        DirectiveType.CODE_DATA,
        DirectiveType.INITIALIZED_DATA,
        DirectiveType.READ_ONLY_DATA,
        DirectiveType.UNINITIALIZED_DATA,
    ]

    def __init__(
        self,
        preprocessed_input_lines: list[SourceLine],
        output_file_path: str,
        offset_address: Number = Number(0),
        program_type: ProgramType = ProgramType.BARE_METAL,
    ) -> None:
        self.preprocessed_input_lines = preprocessed_input_lines
        self.output_file_path = output_file_path
        self.offset_address = offset_address
        self.program_type = program_type

        self._logger = logging.getLogger()

        self._assembly_lines: list[AssemblyLine] = []
        # Mapping of labels to the instruction they point to
        self._label_line_mappings: dict[Label, AssemblyLine] = {}
        # Mapping of labels to the address they point to
        self._label_address_mappings: dict[Label, int] = {}

    @staticmethod
    def _parse_input_lines(input_lines: list[SourceLine]) -> list[AssemblyLine]:
        """Parse input lines into a list of objects while discarding empty lines and preprocessor directives."""
        return [
            AssemblyLine.parse_line(source_line)
            for source_line in input_lines
            if source_line.line.strip() and not source_line.line.strip().startswith("#")
        ]

    def _assign_directives(self) -> None:
        """Assign a directive to each AssemblyLine."""
        if not self._assembly_lines:
            raise ValueError("No assembly lines to assign directives to.")

        # Assume the initial directive is CODE_DATA
        current_directive = DirectiveType.CODE_DATA
        for line in self._assembly_lines:
            if line.directive:
                current_directive = line.directive
            else:
                line.directive = current_directive

    def _reorder_lines_based_on_directive(self) -> None:
        """Reorder lines based on the directive."""
        if not self._assembly_lines:
            raise ValueError("No assembly lines to reorder.")

        self._assembly_lines = sorted(
            self._assembly_lines,
            key=lambda x: self.DIRECTIVE_ORDER.index(x.directive),
        )

    def _remove_comment_lines(self) -> None:
        """Remove comment only assembly lines."""
        self._assembly_lines = [
            line
            for line in self._assembly_lines
            if not isinstance(line, CommentAssemblyLine)
        ]

    def _remove_directive_lines(self) -> None:
        """Remove directive only assembly lines."""
        self._assembly_lines = [
            line
            for line in self._assembly_lines
            if not isinstance(line, DirectiveAssemblyLine)
        ]

    def _remove_label_lines(self) -> None:
        """Remove label only assembly lines."""
        self._assembly_lines = [
            line
            for line in self._assembly_lines
            if not isinstance(line, LabelAssemblyLine)
        ]

    def _expand_instructions(self) -> None:
        """Expand instructions into multiple atomic lines,
        meaning that every instruction will result into a single 32 bit word in the output."""
        unexpanded_lines = self._assembly_lines.copy()
        self._assembly_lines = []
        for line in unexpanded_lines:
            self._assembly_lines.extend(line.expand())

    def _create_label_line_mappings(self) -> None:
        """Create a mapping of labels to the instruction they point to."""
        for idx, line in enumerate(self._assembly_lines):
            if isinstance(line, LabelAssemblyLine):
                # Find the next instruction line that is not a LabelAssemblyLine
                mapping_finished = False
                for next_line in self._assembly_lines[idx:]:
                    if not isinstance(next_line, LabelAssemblyLine):
                        # Check if the label is not already defined
                        if line.label in self._label_line_mappings:
                            raise ValueError(f"Label {line.label} is already defined.")
                        self._label_line_mappings[line.label] = next_line
                        mapping_finished = True
                        break
                if not mapping_finished:
                    raise ValueError(
                        f"Label {line.label} has no instruction to point to."
                    )

    def _create_label_address_mappings(self) -> None:
        """Creates a mapping from label to address using the label line mapping."""
        for label, instruction in self._label_line_mappings.items():
            address = (
                self._assembly_lines.index(instruction) + self.offset_address.value
            )
            self._label_address_mappings[label] = address

    def _apply_label_address_mappings(self) -> None:
        """Replace labels with their addresses in the assembly lines.
        
        For branch instructions, the label is converted to a relative offset
        (target_address - current_address). For other instructions (like jump),
        the absolute address is used.
        """
        for idx, line in enumerate(self._assembly_lines):
            if isinstance(line, InstructionAssemblyLine):
                current_address = idx + self.offset_address.value
                is_branch = isinstance(line.instruction_type, BranchOperation)
                
                for arg in line.arguments:
                    if isinstance(arg, Label):
                        target_address = self._label_address_mappings[arg]
                        if is_branch:
                            # Branch instructions use relative offsets
                            arg.target_address = target_address - current_address
                        else:
                            # Jump instructions use absolute addresses
                            arg.target_address = target_address

    def _process_labels(self) -> None:
        """Process labels in the assembly lines."""
        self._create_label_line_mappings()
        self._remove_label_lines()
        # From this point on, the index in _assembly_lines
        # corresponds to the address of the instruction (ignoring offsets)
        self._create_label_address_mappings()
        self._apply_label_address_mappings()

    def _add_header_instructions(self) -> None:
        """Add header instructions to the beginning of the assembly (before processing)."""

        # Create SourceLine objects for the header instructions
        # We use .dw 0 as a placeholder for the filesize, which only can be calculated at the end
        header_source_lines = [
            SourceLine(
                line="jump Main", source_line_number=0, source_file_name="header"
            ),
            SourceLine(
                line="jump Int", source_line_number=0, source_file_name="header"
            ),
            SourceLine(line=".dw 0", source_line_number=0, source_file_name="header"),
        ]

        # Parse the header lines into assembly lines
        header_assembly_lines = []
        for source_line in header_source_lines:
            header_assembly_lines.append(AssemblyLine.parse_line(source_line))

        # Insert header at the beginning
        self._assembly_lines = header_assembly_lines + self._assembly_lines

    def _update_header_line_count(self) -> None:
        """Update the third header instruction (.dw) with the actual line count."""
        if len(self._assembly_lines) < 3:
            self._logger.warning(
                "Not enough lines to update header line count, this should not happen!"
            )
            return

        # The third instruction should be the .dw instruction
        dw_line = self._assembly_lines[2]
        if (
            hasattr(dw_line, "data_instruction_values")
            and len(dw_line.data_instruction_values) > 0
        ):
            # Update the value with the total line count
            dw_line.data_instruction_values[0] = Number(len(self._assembly_lines))
        else:
            self._logger.warning(
                "Could not update header line count, this should not happen!"
            )

    def _write_output_file(self) -> None:
        """Write the assembly lines as binary strings to the output file."""
        with open(self.output_file_path, "w") as file:
            for line in self._assembly_lines:
                file.write(f"{line.to_binary_string()}\n")

    def assemble(self, add_header: bool = False) -> None:
        """Assemble the preprocessed input file."""

        self._assembly_lines = self._parse_input_lines(self.preprocessed_input_lines)

        if add_header:
            self._add_header_instructions()

        self._expand_instructions()
        self._assign_directives()
        self._reorder_lines_based_on_directive()
        self._remove_directive_lines()
        self._remove_comment_lines()
        self._process_labels()

        if add_header:
            self._update_header_line_count()

        for line in self._assembly_lines:
            self._logger.debug(line)

        self._write_output_file()

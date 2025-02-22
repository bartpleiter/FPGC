from abc import ABC, abstractmethod
from enum import Enum
import logging

from asmpy.utils import split_32bit_to_16bit
from asmpy.models.data_types import (
    DataInstructionType,
    DirectiveType,
    Label,
    Number,
    Register,
    SourceLine,
    ControlOperation,
    MemoryOperation,
    SingleCycleArithmeticOperation,
    MultiCycleArithmeticOperation,
    BranchOperation,
    JumpOperation,
    StringParsableEnum,
    InstructionOpcode,
    BranchOpcode,
    ArithmOpcode,
    ArithmOpcode,
)


class AssemblyLine(ABC):
    """Base class to represent a single line of assembly code"""

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

        self.directive: DirectiveType | None = None

        self._parse_code()

        self._logger = logging.getLogger()

    @staticmethod
    def _matches_data_instruction_type(x: str) -> bool:
        x = x.split()[0]
        try:
            DataInstructionType.from_str(x)
            return True
        except ValueError:
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
    def expand(self) -> list["AssemblyLine"]:
        """Expand the line into one or multiple atomic lines."""
        pass

    @abstractmethod
    def to_binary_string(self) -> str:
        """Convert the line into a binary string."""
        pass

    @abstractmethod
    def __repr__(self):
        pass


class DirectiveAssemblyLine(AssemblyLine):
    """Class to represent a directive line of assembly code"""

    def _parse_code(self):
        parts = self.code_str.split()
        if len(parts) != 1:
            raise ValueError(
                f"Unexpected amount of arguments for directive: {self.code_str}"
            )

        self.directive = DirectiveType.from_str(parts[0])

    def expand(self) -> list["AssemblyLine"]:
        # Directives are already atomic
        return [self]

    def to_binary_string(self) -> str:
        raise NotImplementedError("Directives do not have a binary representation")

    def __repr__(self):
        return f"{self.directive}"


class LabelAssemblyLine(AssemblyLine):
    """Class to represent a label line of assembly code"""

    def _parse_code(self):
        parts = self.code_str.split()
        if len(parts) != 1:
            raise ValueError(
                f"Unexpected amount of arguments for label: {self.code_str}"
            )

        self.label = Label(parts[0][:-1])

    def expand(self) -> list["AssemblyLine"]:
        # Labels are already atomic
        return [self]

    def to_binary_string(self) -> str:
        raise NotImplementedError(
            "Label definitions do not have a binary representation"
        )

    def __repr__(self):
        return f"{repr(self.label)} ({self.directive})"


class InstructionAssemblyLine(AssemblyLine):
    """Class to represent an instruction line of assembly code"""

    INSTRUCTION_OPERATIONS: list[StringParsableEnum] = [
        ControlOperation,
        MemoryOperation,
        SingleCycleArithmeticOperation,
        MultiCycleArithmeticOperation,
        BranchOperation,
        JumpOperation,
    ]

    def _get_instruction_type(self) -> Enum:
        if not self.opcode:
            raise ValueError("Opcode not set")

        for operation in self.INSTRUCTION_OPERATIONS:
            try:
                return operation.from_str(self.opcode)
            except ValueError:
                pass
        raise ValueError(f"Invalid instruction: {self.opcode}")

    def _parse_arguments(self, arguments: list[str]) -> list:
        """Parse the arguments of the instruction. Arguments can be a Register, Label or Number."""
        parsed_arguments = []
        for argument in arguments:
            try:
                parsed_arguments.append(Register.from_str(argument))
            except ValueError:
                try:
                    parsed_arguments.append(Number._from_str(argument))
                except ValueError:
                    parsed_arguments.append(Label(argument))

        return parsed_arguments

    def _parse_code(self):
        self.opcode = self.code_str.split()[0]
        self.instruction_type = self._get_instruction_type()
        self.arguments = self._parse_arguments(self.code_str.split()[1:])

    def expand(self) -> list["AssemblyLine"]:
        if self.instruction_type == SingleCycleArithmeticOperation.LOAD_32_BIT:
            # Split the 32-bit value into load and loadhi instructions
            # Load the low 16 bits first, as this clears the upper 16 bits
            high_16_bits, low_16_bits = split_32bit_to_16bit(self.arguments[0])
            load_low_instruction = InstructionAssemblyLine(
                code_str=f"{SingleCycleArithmeticOperation.LOAD_LOW.value} {low_16_bits} {self.arguments[1]}",
                comment=self.comment,
                original=self.original,
                source_line_number=self.source_line_number,
                source_file_name=self.source_file_name,
            )
            load_high_instruction = InstructionAssemblyLine(
                code_str=f"{SingleCycleArithmeticOperation.LOAD_HIGH.value} {high_16_bits} {self.arguments[1]}",
                comment=self.comment,
                original=self.original,
                source_line_number=self.source_line_number,
                source_file_name=self.source_file_name,
            )
            return [load_low_instruction, load_high_instruction]

        elif self.instruction_type == ControlOperation.ADDRESS_TO_REGISTER:
            # Create a load and loadhi instruction with the label as argument
            # By using a label as argument, load and loadhi should get assigned the correct bits
            load_low_instruction = InstructionAssemblyLine(
                code_str=f"{SingleCycleArithmeticOperation.LOAD_LOW.value} {self.arguments[0]} {self.arguments[1]}",
                comment=self.comment,
                original=self.original,
                source_line_number=self.source_line_number,
                source_file_name=self.source_file_name,
            )
            load_high_instruction = InstructionAssemblyLine(
                code_str=f"{SingleCycleArithmeticOperation.LOAD_HIGH.value} {self.arguments[0]} {self.arguments[1]}",
                comment=self.comment,
                original=self.original,
                source_line_number=self.source_line_number,
                source_file_name=self.source_file_name,
            )
            return [load_low_instruction, load_high_instruction]
        else:
            return [self]

    def _control_operation_to_binary(self) -> str:
        if self.instruction_type == ControlOperation.HALT:
            if self.arguments:
                raise ValueError("HALT does not take arguments")
            return f"{InstructionOpcode.HALT.value}{0:028b}"

        if self.instruction_type == ControlOperation.SAVE_PROGRAM_COUNTER:
            if len(self.arguments) != 1:
                raise ValueError("SAVPC requires one argument")
            if not isinstance(self.arguments[0], Register):
                raise ValueError("SAVPC argument must be a register")
            return f"{InstructionOpcode.SAVPC.value}{0:024b}{self.arguments[0].to_binary()}"

        if self.instruction_type == ControlOperation.CLEAR_CACHE:
            if self.arguments:
                raise ValueError("CCACHE does not take arguments")
            return f"{InstructionOpcode.CCACHE.value}{0:028b}"

        if self.instruction_type == ControlOperation.NOP:
            if self.arguments:
                raise ValueError("NOP does not take arguments")
            return f"{0:032b}"

        if self.instruction_type == ControlOperation.RETURN_INTERRUPT:
            if self.arguments:
                raise ValueError("RETI does not take arguments")
            return f"{InstructionOpcode.RETI.value}{0:028b}"

        if self.instruction_type == ControlOperation.READ_INTERRUPT_ID:
            if len(self.arguments) != 1:
                raise ValueError("READINTID requires one argument")
            if not isinstance(self.arguments[0], Register):
                raise ValueError("READINTID argument must be a register")
            return f"{InstructionOpcode.INTID.value}{0:024b}{self.arguments[0].to_binary()}"

        raise ValueError("Invalid control operation")

    def _memory_operation_to_binary(self) -> str:
        if self.instruction_type == MemoryOperation.READ:
            if len(self.arguments) != 3:
                raise ValueError("READ requires three arguments")
            if not isinstance(self.arguments[0], Number):
                raise ValueError("READ first argument must be a number")
            if not isinstance(self.arguments[1], Register):
                raise ValueError("READ second argument must be a register")
            if not isinstance(self.arguments[2], Register):
                raise ValueError("READ third argument must be a register")
            return f"{InstructionOpcode.READ.value}{self.arguments[0].to_binary(bits=16)}{self.arguments[1].to_binary()}{0:04b}{self.arguments[2].to_binary()}"

        if self.instruction_type == MemoryOperation.WRITE:
            if len(self.arguments) != 3:
                raise ValueError("WRITE requires three arguments")
            if not isinstance(self.arguments[0], Number):
                raise ValueError("WRITE first argument must be a number")
            if not isinstance(self.arguments[1], Register):
                raise ValueError("WRITE second argument must be a register")
            if not isinstance(self.arguments[2], Register):
                raise ValueError("WRITE third argument must be a register")
            return f"{InstructionOpcode.WRITE.value}{self.arguments[0].to_binary(bits=16)}{self.arguments[1].to_binary()}{self.arguments[2].to_binary()}{0:04b}"
        raise ValueError("Invalid memory operation")

    def _single_cycle_arithmetic_operation_to_binary(self) -> str:
        pass

    def _multi_cycle_arithmetic_operation_to_binary(self) -> str:
        pass

    def _branch_operation_to_binary(self) -> str:
        pass

    def _jump_operation_to_binary(self) -> str:
        pass

    def to_binary_string(self) -> str:
        if isinstance(self.instruction_type, ControlOperation):
            instruction_binary_string = self._control_operation_to_binary()
        elif isinstance(self.instruction_type, MemoryOperation):
            instruction_binary_string = self._memory_operation_to_binary()
        elif isinstance(self.instruction_type, SingleCycleArithmeticOperation):
            instruction_binary_string = (
                self._single_cycle_arithmetic_operation_to_binary()
            )
        elif isinstance(self.instruction_type, MultiCycleArithmeticOperation):
            instruction_binary_string = (
                self._multi_cycle_arithmetic_operation_to_binary()
            )
        elif isinstance(self.instruction_type, BranchOperation):
            instruction_binary_string = self._branch_operation_to_binary()
        elif isinstance(self.instruction_type, JumpOperation):
            instruction_binary_string = self._jump_operation_to_binary()
        else:
            raise ValueError("Invalid instruction type")

        comment_string = f" // {self.comment}" if self.comment else ""
        return f"{instruction_binary_string}{comment_string}"

    def __repr__(self):
        return f"{self.instruction_type} {self.arguments} ({self.directive})"


class DataAssemblyLine(AssemblyLine):
    """Class to represent a data line of assembly code"""

    def _parse_code(self):
        self.data_instruction_values: list[Number] = []
        data_instruction_type_str = self.code_str.split()[0]

        self.data_instruction_type = DataInstructionType.from_str(
            data_instruction_type_str
        )

        data_instruction_values = self.code_str.split()[1:]
        for value in data_instruction_values:
            self.data_instruction_values.append(Number._from_str(value))

    def expand(self) -> list["AssemblyLine"]:
        expanded_lines = []
        for value in self.data_instruction_values:
            expanded_lines.append(
                DataAssemblyLine(
                    code_str=f"{self.data_instruction_type.value} {value}",
                    comment=self.comment,
                    original=self.original,
                    source_line_number=self.source_line_number,
                    source_file_name=self.source_file_name,
                )
            )
        return expanded_lines

    def to_binary_string(self) -> str:
        if self.data_instruction_type != DataInstructionType.WORD:
            raise NotImplementedError(
                "Only word data instructions are currently supported"
            )

        if len(self.data_instruction_values) != 1:
            raise ValueError(
                "Data instructions must have exactly one value to convert to binary"
            )

        comment_string = f" // {self.comment}" if self.comment else ""
        return f"{self.data_instruction_values[0].value:032b}{comment_string}"

    def __repr__(self):
        return f"{self.data_instruction_type} {self.data_instruction_values} ({self.directive})"


class CommentAssemblyLine(AssemblyLine):
    """Class to represent a comment line of assembly code"""

    def _parse_code(self):
        pass

    def expand(self) -> list["AssemblyLine"]:
        # Comments are already atomic
        return [self]

    def to_binary_string(self) -> str:
        raise NotImplementedError("Comments do not have a binary representation")

    def __repr__(self):
        return f"# {self.comment}"

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
    ArithOpcode,
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

    INSTRUCTION_OPERATIONS: list[type[StringParsableEnum]] = [
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

    def _parse_arguments(self, arguments: list[str]) -> list[Register | Number | Label]:
        """Parse the arguments of the instruction. Arguments can be a Register, Label or Number."""
        parsed_arguments: list[Register | Number | Label] = []
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

            # Only add loadhi instruction if the high 16 bits are non-zero
            # Since load clears the upper 16 bits, loadhi is unnecessary when high bits are 0
            if high_16_bits.value != 0:
                load_high_instruction = InstructionAssemblyLine(
                    code_str=f"{SingleCycleArithmeticOperation.LOAD_HIGH.value} {high_16_bits} {self.arguments[1]}",
                    comment=self.comment,
                    original=self.original,
                    source_line_number=self.source_line_number,
                    source_file_name=self.source_file_name,
                )
                return [load_low_instruction, load_high_instruction]
            else:
                return [load_low_instruction]

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
            # Note that while not needed, we pad the HALT instruction with ones for increased visibility
            return f"{InstructionOpcode.HALT.value}{(1 << 28) - 1:028b}"

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
        if self.instruction_type == MemoryOperation.PUSH:
            if len(self.arguments) != 1:
                raise ValueError("PUSH requires one argument")
            if not isinstance(self.arguments[0], Register):
                raise ValueError("PUSH argument must be a register")
            breg = self.arguments[0].to_binary()
            return f"{InstructionOpcode.PUSH.value}{0:020b}{breg}{0:04b}"

        if self.instruction_type == MemoryOperation.POP:
            if len(self.arguments) != 1:
                raise ValueError("POP requires one argument")
            if not isinstance(self.arguments[0], Register):
                raise ValueError("POP argument must be a register")
            dreg = self.arguments[0].to_binary()
            return f"{InstructionOpcode.POP.value}{0:024b}{dreg}"

        raise ValueError("Invalid memory operation")

    def _single_cycle_arithmetic_operation_to_binary(self) -> str:
        # Load and loadhi operations have different handling
        if self.instruction_type in (
            SingleCycleArithmeticOperation.LOAD_LOW,
            SingleCycleArithmeticOperation.LOAD_HIGH,
        ):
            if len(self.arguments) != 2:
                raise ValueError(
                    f"{self.instruction_type.value.upper()} requires two arguments"
                )

            # Argument can be either a Number or a Label
            if isinstance(self.arguments[0], Number):
                const16_value = self.arguments[0].value
            elif isinstance(self.arguments[0], Label):
                if self.arguments[0].target_address is None:
                    raise ValueError(f"Label {self.arguments[0]} has no target address")
                # For LOAD_LOW, use lower 16 bits; for LOAD_HIGH, use upper 16 bits
                if self.instruction_type == SingleCycleArithmeticOperation.LOAD_LOW:
                    const16_value = self.arguments[0].target_address & 0xFFFF
                else:  # LOAD_HIGH
                    const16_value = (self.arguments[0].target_address >> 16) & 0xFFFF
            else:
                raise ValueError(
                    f"{self.instruction_type.value.upper()} first argument must be a number or label"
                )

            if not isinstance(self.arguments[1], Register):
                raise ValueError(
                    f"{self.instruction_type.value.upper()} second argument must be a register"
                )

            # Check if value fits in 16 bits unsigned
            if not (0 <= const16_value <= 0xFFFF):
                raise ValueError(
                    f"Value {const16_value} does not fit in 16 bits unsigned"
                )

            const16 = f"{const16_value:016b}"
            dreg = self.arguments[1].to_binary()

            if self.instruction_type == SingleCycleArithmeticOperation.LOAD_LOW:
                return f"{InstructionOpcode.ARITHC.value}{ArithOpcode.LOAD.value}{const16}{dreg}{dreg}"
            else:  # LOAD_HIGH
                return f"{InstructionOpcode.ARITHC.value}{ArithOpcode.LOADHI.value}{const16}{dreg}{dreg}"

        # NOT operation has only 2 arguments (source and destination registers)
        elif self.instruction_type == SingleCycleArithmeticOperation.NOT:
            if len(self.arguments) != 2:
                raise ValueError("NOT requires two arguments")
            if not isinstance(self.arguments[0], Register):
                raise ValueError("NOT first argument must be a register")
            if not isinstance(self.arguments[1], Register):
                raise ValueError("NOT second argument must be a register")

            areg = self.arguments[0].to_binary()
            dreg = self.arguments[1].to_binary()

            return f"{InstructionOpcode.ARITH.value}{ArithOpcode.NOTA.value}000000000000{areg}{0:04b}{dreg}"

        # All other arithmetic operations have 3 arguments
        else:
            if len(self.arguments) != 3:
                raise ValueError(
                    f"{self.instruction_type.value.upper()} requires three arguments"
                )

            if not isinstance(self.arguments[0], Register):
                raise ValueError(
                    f"{self.instruction_type.value.upper()} first argument must be a register"
                )
            if not isinstance(self.arguments[2], Register):
                raise ValueError(
                    f"{self.instruction_type.value.upper()} third argument must be a register"
                )

            areg = self.arguments[0].to_binary()
            dreg = self.arguments[2].to_binary()

            # Map instruction to arithmetic opcode
            opcode_map = {
                SingleCycleArithmeticOperation.OR: ArithOpcode.OR,
                SingleCycleArithmeticOperation.AND: ArithOpcode.AND,
                SingleCycleArithmeticOperation.XOR: ArithOpcode.XOR,
                SingleCycleArithmeticOperation.ADD: ArithOpcode.ADD,
                SingleCycleArithmeticOperation.SUBTRACT: ArithOpcode.SUB,
                SingleCycleArithmeticOperation.SHIFT_LEFT: ArithOpcode.SHIFTL,
                SingleCycleArithmeticOperation.SHIFT_RIGHT: ArithOpcode.SHIFTR,
                SingleCycleArithmeticOperation.SHIFT_RIGHT_SIGNED: ArithOpcode.SHIFTRS,
                SingleCycleArithmeticOperation.SET_LESS_THAN: ArithOpcode.SLT,
                SingleCycleArithmeticOperation.SET_LESS_THAN_UNSIGNED: ArithOpcode.SLTU,
            }

            if self.instruction_type not in opcode_map:
                raise ValueError(
                    f"Unsupported arithmetic operation: {self.instruction_type}"
                )

            arith_opcode = opcode_map[self.instruction_type].value

            # Check if second argument is register or constant
            if isinstance(self.arguments[1], Register):
                breg = self.arguments[1].to_binary()
                return f"{InstructionOpcode.ARITH.value}{arith_opcode}{0:012b}{areg}{breg}{dreg}"
            if isinstance(self.arguments[1], Number):
                const_value = self.arguments[1].value
                if not (-32768 <= const_value <= 32767):
                    raise ValueError(
                        f"Constant {const_value} does not fit in 16 bits signed"
                    )
                const16 = f"{const_value & 0xFFFF:016b}"
                return f"{InstructionOpcode.ARITHC.value}{arith_opcode}{const16}{areg}{dreg}"
            raise ValueError(
                f"{self.instruction_type.value.upper()} second argument must be a register or number"
            )

    def _multi_cycle_arithmetic_operation_to_binary(self) -> str:
        if len(self.arguments) != 3:
            raise ValueError(
                f"{self.instruction_type.value.upper()} requires three arguments"
            )
        if not isinstance(self.arguments[0], Register):
            raise ValueError("First argument must be register")
        if not isinstance(self.arguments[2], Register):
            raise ValueError("Third argument must be register")
        areg = self.arguments[0].to_binary()
        dreg = self.arguments[2].to_binary()
        opcode_map = {
            MultiCycleArithmeticOperation.MULTIPLY_SIGNED: ArithmOpcode.MULTS,
            MultiCycleArithmeticOperation.MULTIPLY_UNSIGNED: ArithmOpcode.MULTU,
            MultiCycleArithmeticOperation.MULTIPLY_FIXED_POINT: ArithmOpcode.MULTFP,
            MultiCycleArithmeticOperation.DIVIDE_SIGNED: ArithmOpcode.DIVS,
            MultiCycleArithmeticOperation.DIVIDE_UNSIGNED: ArithmOpcode.DIVU,
            MultiCycleArithmeticOperation.DIVIDE_FIXED_POINT: ArithmOpcode.DIVFP,
            MultiCycleArithmeticOperation.MODULO_SIGNED: ArithmOpcode.MODS,
            MultiCycleArithmeticOperation.MODULO_UNSIGNED: ArithmOpcode.MODU,
        }
        arithm_opcode = opcode_map[self.instruction_type].value
        if isinstance(self.arguments[1], Register):
            breg = self.arguments[1].to_binary()
            # Register form should use ARITHMC (0010) per original assembler
            return f"{InstructionOpcode.ARITHMC.value}{arithm_opcode}{0:012b}{areg}{breg}{dreg}"
        if isinstance(self.arguments[1], Number):
            const_value = self.arguments[1].value
            if not (-32768 <= const_value <= 32767):
                raise ValueError(
                    f"Constant {const_value} does not fit in 16 bits signed"
                )
            const16 = f"{const_value & 0xFFFF:016b}"
            # Constant form should use ARITHM (0011)
            return (
                f"{InstructionOpcode.ARITHM.value}{arithm_opcode}{const16}{areg}{dreg}"
            )
        raise ValueError("Second argument must be register or number")

    def _branch_operation_to_binary(self) -> str:
        if len(self.arguments) != 3:
            raise ValueError(
                f"{self.instruction_type.value.upper()} requires three arguments"
            )
        if not isinstance(self.arguments[0], Register) or not isinstance(
            self.arguments[1], Register
        ):
            raise ValueError("First two arguments must be registers")
        if not isinstance(self.arguments[2], Number) and not isinstance(
            self.arguments[2], Label
        ):
            raise ValueError(
                "Third argument must be a number or label (resolved to number)"
            )
        # If label, we expect assembler to have resolved target_address
        offset_value: int
        if isinstance(self.arguments[2], Label):
            if self.arguments[2].target_address is None:
                raise ValueError("Branch label unresolved")
            # Branch immediate is relative? Legacy code uses an offset; assume immediate is already relative.
            offset_value = self.arguments[2].target_address
        else:
            offset_value = self.arguments[2].value
        if not (-32768 <= offset_value <= 32767):
            raise ValueError("Branch offset does not fit in 16 bits signed")
        const16 = f"{offset_value & 0xFFFF:016b}"
        areg = self.arguments[0].to_binary()
        breg = self.arguments[1].to_binary()
        opcode_map = {
            BranchOperation.BRANCH_EQUAL: (BranchOpcode.BEQ, False),
            BranchOperation.BRANCH_NOT_EQUAL: (BranchOpcode.BNE, False),
            BranchOperation.BRANCH_GREATER_UNSIGNED: (BranchOpcode.BGT, False),
            BranchOperation.BRANCH_GREATER_EQUAL_UNSIGNED: (BranchOpcode.BGE, False),
            BranchOperation.BRANCH_LESS_UNSIGNED: (BranchOpcode.BLT, False),
            BranchOperation.BRANCH_LESS_EQUAL_UNSIGNED: (BranchOpcode.BLE, False),
            BranchOperation.BRANCH_GREATER_SIGNED: (BranchOpcode.BGT, True),
            BranchOperation.BRANCH_GREATER_EQUAL_SIGNED: (BranchOpcode.BGE, True),
            BranchOperation.BRANCH_LESS_SIGNED: (BranchOpcode.BLT, True),
            BranchOperation.BRANCH_LESS_EQUAL_SIGNED: (BranchOpcode.BLE, True),
        }
        branch_opcode, signed_flag = opcode_map[self.instruction_type]
        signed_bit = "1" if signed_flag else "0"
        return f"{InstructionOpcode.BRANCH.value}{const16}{areg}{breg}{branch_opcode.value}{signed_bit}"

    def _jump_operation_to_binary(self) -> str:
        if self.instruction_type == JumpOperation.JUMP:
            if len(self.arguments) != 1:
                raise ValueError("JUMP requires one argument")
            target = self.arguments[0]
            if isinstance(target, Label):
                if target.target_address is None:
                    raise ValueError("Unresolved jump label")
                address = target.target_address
            elif isinstance(target, Number):
                address = target.value
            else:
                raise ValueError("JUMP argument must be number or label")
            if not (0 <= address < (1 << 27)):
                raise ValueError("Jump address must fit in 27 bits unsigned")
            const27 = f"{address:027b}"
            return f"{InstructionOpcode.JUMP.value}{const27}0"
        if self.instruction_type == JumpOperation.JUMP_OFFSET:
            if len(self.arguments) != 1:
                raise ValueError("JUMPO requires one argument")
            offset = self.arguments[0]
            if not isinstance(offset, Number):
                raise ValueError("JUMPO argument must be number")
            if not (0 <= offset.value < (1 << 27)):
                raise ValueError("Jump offset must fit in 27 bits unsigned")
            const27 = f"{offset.value:027b}"
            return f"{InstructionOpcode.JUMP.value}{const27}1"
        if self.instruction_type in (
            JumpOperation.JUMP_REGISTER,
            JumpOperation.JUMP_REGISTER_OFFSET,
        ):
            if len(self.arguments) != 2:
                raise ValueError(
                    "JUMPR/JUMPRO require two arguments (offset, register)"
                )
            if not isinstance(self.arguments[0], Number):
                raise ValueError("First argument must be number (offset)")
            if not isinstance(self.arguments[1], Register):
                raise ValueError("Second argument must be register")
            offset = self.arguments[0].value
            if not (-32768 <= offset <= 32767):
                raise ValueError("Offset must fit in 16 bits signed")
            const16 = f"{offset & 0xFFFF:016b}"
            breg = self.arguments[1].to_binary()
            offset_flag = (
                "1"
                if self.instruction_type == JumpOperation.JUMP_REGISTER_OFFSET
                else "0"
            )
            return f"{InstructionOpcode.JUMPR.value}{const16}{0:04b}{breg}{BranchOpcode.BEQ.value}{offset_flag}"
        raise ValueError("Invalid jump operation")

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

    def _parse_code(self) -> None:
        self.data_instruction_values: list[Number] = []
        data_instruction_type_str = self.code_str.split()[0]

        self.data_instruction_type = DataInstructionType.from_str(
            data_instruction_type_str
        )

        # Get everything after the instruction type
        rest_of_line = self.code_str[len(data_instruction_type_str) :].strip()

        # Handle .dsw (string spaced) - each character becomes a separate word
        if self.data_instruction_type == DataInstructionType.STRING_SPACED:
            self._parse_string_values(rest_of_line)
        else:
            # Original behavior for .dw and other data types
            data_instruction_values = rest_of_line.split()
            for value in data_instruction_values:
                self.data_instruction_values.append(Number._from_str(value))

    def _parse_string_values(self, rest_of_line: str) -> None:
        """Parse a string literal and convert each character to a Number."""
        # Find the quoted string
        if '"' not in rest_of_line:
            raise ValueError(
                f"Expected quoted string for .dsw directive: {rest_of_line}"
            )

        # Find the start and end of the string
        first_quote = rest_of_line.index('"')
        last_quote = rest_of_line.rindex('"')

        if first_quote == last_quote:
            raise ValueError(f"Unterminated string literal: {rest_of_line}")

        string_content = rest_of_line[first_quote + 1 : last_quote]

        # Process escape sequences and convert each character to a number
        i = 0
        while i < len(string_content):
            if string_content[i] == "\\" and i + 1 < len(string_content):
                # Handle escape sequences
                next_char = string_content[i + 1]
                if next_char == "n":
                    self.data_instruction_values.append(Number(ord("\n")))
                    i += 2
                elif next_char == "r":
                    self.data_instruction_values.append(Number(ord("\r")))
                    i += 2
                elif next_char == "t":
                    self.data_instruction_values.append(Number(ord("\t")))
                    i += 2
                elif next_char == "0":
                    self.data_instruction_values.append(Number(0))
                    i += 2
                elif next_char == "\\":
                    self.data_instruction_values.append(Number(ord("\\")))
                    i += 2
                elif next_char == '"':
                    self.data_instruction_values.append(Number(ord('"')))
                    i += 2
                elif next_char.isdigit():
                    # Octal escape sequence (e.g., \000)
                    octal_str = ""
                    j = i + 1
                    while (
                        j < len(string_content)
                        and len(octal_str) < 3
                        and string_content[j].isdigit()
                    ):
                        octal_str += string_content[j]
                        j += 1
                    self.data_instruction_values.append(Number(int(octal_str, 8)))
                    i = j
                else:
                    # Unknown escape, treat backslash as literal
                    self.data_instruction_values.append(Number(ord("\\")))
                    i += 1
            else:
                # Regular character
                self.data_instruction_values.append(Number(ord(string_content[i])))
                i += 1

    def expand(self) -> list["AssemblyLine"]:
        expanded_lines: list[AssemblyLine] = []
        for value in self.data_instruction_values:
            expanded_lines.append(
                DataAssemblyLine(
                    code_str=f"{DataInstructionType.WORD.value} {value}",
                    comment=self.comment,
                    original=self.original,
                    source_line_number=self.source_line_number,
                    source_file_name=self.source_file_name,
                )
            )
        return expanded_lines

    def to_binary_string(self) -> str:
        if self.data_instruction_type not in (
            DataInstructionType.WORD,
            DataInstructionType.STRING_SPACED,
        ):
            raise NotImplementedError(
                "Only word and string spaced data instructions are currently supported"
            )

        if len(self.data_instruction_values) != 1:
            raise ValueError(
                "Data instructions must have exactly one value to convert to binary"
            )

        comment_string = f" // {self.comment}" if self.comment else ""
        return (
            f"{self.data_instruction_values[0].value & 0xFFFFFFFF:032b}{comment_string}"
        )

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

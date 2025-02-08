from enum import Enum


class ProgramType(Enum):
    """Types of programs to assemble"""

    BDOS = "bdos"
    UserBDOS = "userbdos"
    BareMetal = "baremetal"
    Headerless = "headerless"


class Number:
    def __init__(self, input_str: str) -> None:
        self.original = input_str.strip()
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

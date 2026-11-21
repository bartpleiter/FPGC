"""The unit bench of the I/O tests: the common one, with the I/O devices' RTL."""

from dataclasses import dataclass, field

from ..common.unit_bench import UnitBench as CommonUnitBench
from ..common.unit_bench import Vector
from ..common.verilog import Area
from .verilog import IO

__all__ = ["UnitBench", "Vector"]


@dataclass
class UnitBench(CommonUnitBench):
    area: Area = field(default=IO, kw_only=True)

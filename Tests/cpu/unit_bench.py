"""The unit bench of the CPU's tests: the common one, with the CPU's RTL and folders."""

from dataclasses import dataclass, field

from ..common.unit_bench import UnitBench as CommonUnitBench
from ..common.unit_bench import Vector
from ..common.verilog import Area
from .verilog import CPU

__all__ = ["UnitBench", "Vector"]


@dataclass
class UnitBench(CommonUnitBench):
    area: Area = field(default=CPU, kw_only=True)

"""The unit bench of the system tests: the common one, with the system's RTL."""

from dataclasses import dataclass, field

from ..common.unit_bench import UnitBench as CommonUnitBench
from ..common.unit_bench import Vector
from ..common.verilog import Area
from .verilog import SYSTEM

__all__ = ["UnitBench", "Vector"]


@dataclass
class UnitBench(CommonUnitBench):
    area: Area = field(default=SYSTEM, kw_only=True)

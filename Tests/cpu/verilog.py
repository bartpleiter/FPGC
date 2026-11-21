"""The CPU's RTL and test folders, for compiling and running its testbenches."""

from pathlib import Path

from fpgc.config import CONFIG

from ..common.verilog import REPO, Area, run_vvp
from ..common.verilog import compile_tb as _compile_tb

CFG = CONFIG.cpu
BUILD_DIR = REPO / CFG.build_dir
WAVE_DIR = REPO / CFG.wave_dir
MODULES_DIR = REPO / CFG.modules_dir
RTL_FILES = [REPO / f for f in CFG.rtl_files]

CPU = Area(
    rtl_files=tuple(RTL_FILES),
    include_dirs=(MODULES_DIR,),
    build_dir=BUILD_DIR,
    wave_dir=WAVE_DIR,
)

__all__ = ["BUILD_DIR", "CPU", "MODULES_DIR", "REPO", "RTL_FILES", "WAVE_DIR"]
__all__ += ["compile_tb", "run_vvp"]


def compile_tb(
    tb: Path,
    sources: list[Path],
    out: Path,
    parameters: dict[str, int] | None = None,
) -> Path:
    """Compile a CPU testbench, which finds its .vh files in Tests/cpu/modules."""
    return _compile_tb(tb, sources, out, parameters, CPU.include_dirs)

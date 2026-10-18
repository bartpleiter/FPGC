"""Compile and run Verilog testbenches with Icarus for the tests."""

import subprocess
from pathlib import Path

from fpgc.config import CONFIG

REPO = Path(__file__).resolve().parents[2]
CFG = CONFIG.cpu
BUILD_DIR = REPO / CFG.build_dir
WAVE_DIR = REPO / CFG.wave_dir
MODULES_DIR = REPO / CFG.modules_dir
RTL_FILES = [REPO / f for f in CFG.rtl_files]


def compile_tb(tb: Path, sources: list[Path], out: Path) -> Path:
    """Compile testbench `tb` with `sources` into `out`."""
    out.parent.mkdir(parents=True, exist_ok=True)
    _call(
        "iverilog",
        "-g2012",
        "-Wall",
        "-Wno-timescale",
        "-I",
        MODULES_DIR,
        "-s",
        tb.stem,
        "-o",
        out,
        tb,
        *sources,
    )
    return out


def run_vvp(vvp: Path, **plusargs: object) -> str:
    """Run a compiled testbench with `+name=value` plusargs. Returns its output."""
    return _call("vvp", "-n", vvp, *(f"+{k}={v}" for k, v in plusargs.items()))


def _call(*cmd: object) -> str:
    args = [str(c) for c in cmd]
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    output = result.stdout + result.stderr
    if result.returncode != 0:
        raise RuntimeError(f"{' '.join(args)} failed:\n{output}")
    return output

"""Compile and run Verilog testbenches with Icarus for the tests."""

import os
import subprocess
from pathlib import Path

from fpgc.config import CONFIG

REPO = Path(__file__).resolve().parents[2]
CFG = CONFIG.cpu
BUILD_DIR = REPO / CFG.build_dir
WAVE_DIR = REPO / CFG.wave_dir
MODULES_DIR = REPO / CFG.modules_dir
RTL_FILES = [REPO / f for f in CFG.rtl_files]


def compile_tb(
    tb: Path,
    sources: list[Path],
    out: Path,
    parameters: dict[str, int] | None = None,
) -> Path:
    """Compile testbench `tb` with `sources` into `out`, with its `parameters` set.

    The tests run in parallel, and each worker compiles the testbenches it needs. So
    Icarus writes to a file of this process first, which then replaces `out` in one
    step: a simulation that starts meanwhile never sees half a file.
    """
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_name(f"{out.name}.{os.getpid()}")
    _call(
        "iverilog",
        "-g2012",
        "-Wall",
        "-Wno-timescale",
        "-I",
        MODULES_DIR,
        "-s",
        tb.stem,
        *(f"-P{tb.stem}.{k}={v}" for k, v in (parameters or {}).items()),
        "-o",
        tmp,
        tb,
        *sources,
    )
    tmp.replace(out)
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

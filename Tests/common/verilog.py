"""Compile and run Verilog testbenches with Icarus, for the tests of every area."""

import os
import subprocess
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
COMMON_DIR = Path(__file__).parent  # tb_unit.vh and the other shared .vh files


@dataclass(frozen=True)
class Area:
    """The RTL of one area (the CPU, the I/O devices, ...) and its test folders."""

    rtl_files: tuple[Path, ...]  # absolute paths
    include_dirs: tuple[Path, ...]  # for .vh files, besides COMMON_DIR
    build_dir: Path  # absolute
    wave_dir: Path  # absolute: the demos' waveforms


def compile_tb(
    tb: Path,
    sources: list[Path] | tuple[Path, ...],
    out: Path,
    parameters: dict[str, int | str] | None = None,
    include_dirs: list[Path] | tuple[Path, ...] = (),
    defines: list[str] | tuple[str, ...] = (),
) -> Path:
    """Compile testbench `tb` with `sources` into `out`, with its `parameters` set,
    and the macros of `defines` defined.

    The tests run in parallel, and each worker compiles the testbenches it needs. So
    Icarus writes to a file of this process first, which then replaces `out` in one
    step: a simulation that starts meanwhile never sees half a file.
    """
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_name(f"{out.name}.{os.getpid()}")
    includes = [arg for d in (*include_dirs, COMMON_DIR) for arg in ("-I", d)]
    _call(
        "iverilog",
        "-g2012",
        "-Wall",
        "-Wno-timescale",
        *includes,
        "-s",
        tb.stem,
        *(f"-P{tb.stem}.{k}={v}" for k, v in (parameters or {}).items()),
        *(f"-D{d}" for d in defines),
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

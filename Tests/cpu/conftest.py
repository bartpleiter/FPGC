"""Fixtures for the CPU tests."""

from pathlib import Path

import pytest

from .iss_comparison import compile_b32p4
from .verilog import WAVE_DIR


@pytest.fixture(scope="session")
def b32p4_sim() -> Path:
    """The ISS comparison testbench with the CPU, compiled once per test session."""
    return compile_b32p4()


@pytest.fixture
def wave_dir(request: pytest.FixtureRequest) -> Path | None:
    """Where the ISS comparisons dump their waveform: build/cpu/wave/ with --wave, else None."""
    return WAVE_DIR if request.config.getoption("wave") else None

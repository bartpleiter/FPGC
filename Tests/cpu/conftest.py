"""Fixtures for the CPU tests."""

from pathlib import Path

import pytest

from .lockstep import compile_b32p4
from .verilog import WAVE_DIR


@pytest.fixture(scope="session")
def b32p4_sim() -> Path:
    """The lockstep testbench with the CPU, compiled once per test session."""
    return compile_b32p4()


@pytest.fixture
def wave_dir(request: pytest.FixtureRequest) -> Path | None:
    """Where lockstep runs dump their waveform: build/cpu/wave/ with --wave, else None."""
    return WAVE_DIR if request.config.getoption("wave") else None

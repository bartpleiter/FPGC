"""Fixtures for the CPU tests."""

from pathlib import Path

import pytest

from .verilog import WAVE_DIR


@pytest.fixture
def wave_dir(request: pytest.FixtureRequest) -> Path | None:
    """Where the ISS comparisons dump their waveform: build/cpu/wave/ with --wave, else None."""
    return WAVE_DIR if request.config.getoption("wave") else None

"""Command-line options for the tests."""

from pathlib import Path

import pytest


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("cpu", "CPU tests")
    group.addoption(
        "--wave",
        action="store_true",
        help="Dump a waveform of every lockstep run, to build/cpu/wave/.",
    )
    group.addoption(
        "--program",
        action="append",
        default=[],
        type=Path,
        help="Also run this program as a demo in lockstep, with a waveform.",
    )

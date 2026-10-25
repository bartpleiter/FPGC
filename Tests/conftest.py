"""Command-line options for the tests."""

from pathlib import Path

import pytest


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("cpu", "CPU tests")
    group.addoption(
        "--wave",
        action="store_true",
        help="Dump a waveform of every ISS comparison, to build/cpu/wave/.",
    )
    group.addoption(
        "--program",
        action="append",
        default=[],
        type=Path,
        help="Also run this program as a demo, compared with the ISS, with a waveform.",
    )
    group.addoption(
        "--fetch-latency",
        default="0",
        help="The latency of the instruction memory for the demos: cycles, or rand.",
    )
    group.addoption(
        "--data-latency",
        default="0",
        help="The latency of the data memory for the demos: cycles, or rand.",
    )
    group.addoption(
        "--latency-seed",
        type=int,
        default=0,
        help="The seed of the random latencies for the demos.",
    )
    group.addoption(
        "--seeds",
        type=int,
        default=100,
        help="The number of random programs compared with the ISS (default 100).",
    )


def pytest_generate_tests(metafunc: pytest.Metafunc) -> None:
    if "random_seed" in metafunc.fixturenames:
        metafunc.parametrize("random_seed", range(metafunc.config.getoption("seeds")))

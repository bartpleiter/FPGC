"""The tests on the board run only with --board (`fpgc board test`), one at a time,
over one serial port."""

from collections.abc import Iterator
from pathlib import Path

import pytest
import serial

from fpgc.config import CONFIG
from uartload.board import Board


def pytest_ignore_collect(collection_path: Path, config: pytest.Config) -> bool:
    return not config.getoption("board")


@pytest.fixture(scope="session")
def board() -> Iterator[Board]:
    """The board, which must have the current `fpgc system build` on it."""
    with serial.Serial(CONFIG.board.port, CONFIG.board.baud, timeout=0.1) as link:
        yield Board(link, echo=False)

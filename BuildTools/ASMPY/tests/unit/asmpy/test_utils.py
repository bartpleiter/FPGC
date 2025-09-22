from unittest.mock import patch

import pytest

from asmpy.utils import parse_args


def test_parse_args():
    # Arrange
    test_args = ["input.asm", "output.list"]

    # Act
    with patch("sys.argv", ["app.py"] + test_args):
        args = parse_args()

    # Assert
    assert args.file == "input.asm"
    assert args.output == "output.list"


def test_parse_args_missing_file():
    # Arrange
    test_args = []

    # Act & Assert
    with patch("sys.argv", ["app.py"] + test_args):
        with pytest.raises(SystemExit):
            parse_args()


def test_parse_args_missing_output():
    # Arrange
    test_args = ["input.asm"]

    # Act & Assert
    with patch("sys.argv", ["app.py"] + test_args):
        with pytest.raises(SystemExit):
            parse_args()

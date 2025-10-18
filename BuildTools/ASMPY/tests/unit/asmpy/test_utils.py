from unittest.mock import patch
import tempfile
import os

import pytest

from asmpy.utils import parse_args, read_input_file, split_32bit_to_16bit
from asmpy.models.data_types import Number
from pathlib import Path


def test_parse_args():
    """Test parse_args with required arguments."""
    # Arrange
    test_args = ["input.asm", "output.list"]

    # Act
    with patch("sys.argv", ["app.py"] + test_args):
        args = parse_args()

    # Assert
    assert args.file == "input.asm"
    assert args.output == "output.list"
    assert args.log_level == "info"
    assert args.log_details is False
    assert args.header is False
    assert args.offset == "0"


def test_parse_args_with_optional_arguments():
    """Test parse_args with optional arguments."""
    # Arrange
    test_args = [
        "input.asm",
        "output.list",
        "-l",
        "debug",
        "-d",
        "-h",
        "-o",
        "0x1000",
    ]

    # Act
    with patch("sys.argv", ["app.py"] + test_args):
        args = parse_args()

    # Assert
    assert args.file == "input.asm"
    assert args.output == "output.list"
    assert args.log_level == "debug"
    assert args.log_details is True
    assert args.header is True
    assert args.offset == "0x1000"


def test_parse_args_missing_file():
    """Test parse_args with missing file argument exits."""
    # Arrange
    test_args = []

    # Act & Assert
    with patch("sys.argv", ["app.py"] + test_args):
        with pytest.raises(SystemExit):
            parse_args()


def test_parse_args_missing_output():
    """Test parse_args with missing output argument exits."""
    # Arrange
    test_args = ["input.asm"]

    # Act & Assert
    with patch("sys.argv", ["app.py"] + test_args):
        with pytest.raises(SystemExit):
            parse_args()


def test_read_input_file():
    """Test read_input_file successfully reads file content."""
    # Arrange
    file_content = "load 1 r1\nadd r1 r2 r3\nhalt\n"
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm")
    try:
        temp_file.write(file_content)
        temp_file.close()
        file_path = Path(temp_file.name)

        # Act
        result = read_input_file(file_path)

        # Assert
        assert len(result) == 3
        assert result[0].line == "load 1 r1"
        assert result[0].source_line_number == 1
        assert result[1].line == "add r1 r2 r3"
        assert result[1].source_line_number == 2
        assert result[2].line == "halt"
        assert result[2].source_line_number == 3
    finally:
        os.unlink(temp_file.name)


def test_read_input_file_not_found():
    """Test read_input_file raises FileNotFoundError for non-existent file."""
    # Arrange
    file_path = Path("nonexistent.asm")

    # Act & Assert
    with pytest.raises(FileNotFoundError) as exc_info:
        read_input_file(file_path)
    assert "Input file not found" in str(exc_info.value)


def test_split_32bit_to_16bit_positive():
    """Test splitting positive 32-bit number into two 16-bit numbers."""
    # Arrange
    num = Number("0x12345678")

    # Act
    upper, lower = split_32bit_to_16bit(num)

    # Assert
    assert upper.value == 0x1234
    assert lower.value == 0x5678
    assert "[31:16]" in upper.original
    assert "[15:0]" in lower.original


def test_split_32bit_to_16bit_zero():
    """Test splitting zero value."""
    # Arrange
    num = Number("0")

    # Act
    upper, lower = split_32bit_to_16bit(num)

    # Assert
    assert upper.value == 0
    assert lower.value == 0


def test_split_32bit_to_16bit_negative():
    """Test splitting negative 32-bit number."""
    # Arrange
    num = Number("-1")

    # Act
    upper, lower = split_32bit_to_16bit(num)

    # Assert
    # -1 in 32-bit two's complement is 0xFFFFFFFF
    assert upper.value == 0xFFFF
    assert lower.value == 0xFFFF


def test_split_32bit_to_16bit_max_positive():
    """Test splitting maximum positive 32-bit value."""
    # Arrange
    num = Number("0xFFFFFFFF")

    # Act
    upper, lower = split_32bit_to_16bit(num)

    # Assert
    assert upper.value == 0xFFFF
    assert lower.value == 0xFFFF


def test_split_32bit_to_16bit_invalid_too_large():
    """Test split_32bit_to_16bit raises ValueError for too large number."""
    # Arrange
    num = Number("0x100000000")  # 33-bit number

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        split_32bit_to_16bit(num)
    assert "must fit in 32 bits" in str(exc_info.value)


def test_split_32bit_to_16bit_invalid_too_small():
    """Test split_32bit_to_16bit raises ValueError for too small number."""
    # Arrange
    # Create a number smaller than -0x80000000
    num = Number(value=-0x80000001, original="-2147483649")

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        split_32bit_to_16bit(num)
    assert "must fit in 32 bits" in str(exc_info.value)

import pytest
from asmpy.models import Number


def test_number_decimal():
    # Arrange & Act
    num = Number("42")

    # Assert
    assert num.original == "42"
    assert num.value == 42


def test_number_decimal_negative():
    # Arrange & Act
    num = Number("-42")

    # Assert
    assert num.original == "-42"
    assert num.value == -42


def test_number_binary():
    # Arrange & Act
    num = Number("0b1010")

    # Assert
    assert num.original == "0b1010"
    assert num.value == 10


def test_number_hexadecimal():
    # Arrange & Act
    num = Number("0x1A")

    # Assert
    assert num.original == "0x1A"
    assert num.value == 26


def test_number_binary_hexadecimal_uppercase():
    # Arrange & Act
    hex_num = Number("0X1A")
    bin_num = Number("0B1010")

    # Assert
    assert hex_num.original == "0X1A"
    assert hex_num.value == 26
    assert bin_num.original == "0B1010"
    assert bin_num.value == 10


def test_number_invalid():
    # Arrange, Act & Assert
    with pytest.raises(ValueError):
        Number("invalid")


def test_number_int_conversion():
    # Arrange & Act
    num = Number("0x1A")

    # Assert
    assert int(num) == 26


def test_number_str_conversion():
    # Arrange & Act
    num = Number("0b1010")

    # Assert
    assert str(num) == "10"


def test_number_to_binary_positive():
    """Test converting positive number to binary string."""
    # Arrange
    num = Number("42")

    # Act
    result = num.to_binary(8)

    # Assert
    assert result == "00101010"


def test_number_to_binary_negative():
    """Test converting negative number to binary string (two's complement)."""
    # Arrange
    num = Number("-1")

    # Act
    result = num.to_binary(8)

    # Assert
    assert result == "11111111"  # -1 in 8-bit two's complement


def test_number_to_binary_overflow_negative():
    """Test that negative overflow raises ValueError."""
    # Arrange
    num = Number("-129")  # Too small for 8-bit signed

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        num.to_binary(8)
    assert "must fit (signed) in 8 bits" in str(exc_info.value)

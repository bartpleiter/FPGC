import tempfile
import os
from pathlib import Path
import pytest
from asmpy.preprocessor import Preprocessor
from asmpy.models.data_types import SourceLine
from asmpy.models.preprocessor import (
    DefinePreprocessorDirective,
    IncludePreprocessorDirective,
    PreprocessorDirective,
    Define,
    Include,
)


def test_define_init():
    """Test that Define object is created correctly."""
    # Arrange & Act
    define = Define("MY_CONST", "42")

    # Assert
    assert define.name == "MY_CONST"
    assert define.value == "42"


def test_include_init():
    """Test that Include object is created correctly."""
    # Arrange & Act
    include = Include('"lib.asm"')

    # Assert
    assert include.file == "lib.asm"
    assert include.file_name == "lib.asm"


def test_include_init_without_quotes():
    """Test that Include handles file names without quotes."""
    # Arrange & Act
    include = Include("lib.asm")

    # Assert
    assert include.file == "lib.asm"


def test_parse_include_directive():
    """Test parsing of include preprocessor directive."""
    # Arrange
    source_line = SourceLine(
        line='#include "test.asm"',
        source_line_number=1,
        source_file_name="main.asm",
    )

    # Act
    directive = PreprocessorDirective.parse_line(source_line)

    # Assert
    assert isinstance(directive, IncludePreprocessorDirective)
    assert directive.include_file.file_name == "test.asm"
    assert directive.code_str == '#include "test.asm"'
    assert directive.source_line_number == 1


def test_parse_define_directive():
    """Test parsing of define preprocessor directive."""
    # Arrange
    source_line = SourceLine(
        line="#define STACK_SIZE 256",
        source_line_number=5,
        source_file_name="config.asm",
    )

    # Act
    directive = PreprocessorDirective.parse_line(source_line)

    # Assert
    assert isinstance(directive, DefinePreprocessorDirective)
    assert directive.define.name == "STACK_SIZE"
    assert directive.define.value == "256"


def test_parse_directive_with_comment():
    """Test parsing of preprocessor directive with comment."""
    # Arrange
    source_line = SourceLine(
        line="#define MAX 100 ; Maximum value",
        source_line_number=1,
        source_file_name="test.asm",
    )

    # Act
    directive = PreprocessorDirective.parse_line(source_line)

    # Assert
    assert isinstance(directive, DefinePreprocessorDirective)
    assert directive.define.name == "MAX"
    assert directive.define.value == "100"
    assert directive.comment == "Maximum value"


def test_parse_invalid_directive():
    """Test that invalid preprocessor directive raises ValueError."""
    # Arrange
    source_line = SourceLine(
        line="#invalid directive",
        source_line_number=1,
        source_file_name="test.asm",
    )

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        PreprocessorDirective.parse_line(source_line)
    assert "Invalid preprocessor directive" in str(exc_info.value)


def test_parse_invalid_include_format():
    """Test that invalid include format raises ValueError."""
    # Arrange
    source_line = SourceLine(
        line="#include",
        source_line_number=1,
        source_file_name="test.asm",
    )

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        PreprocessorDirective.parse_line(source_line)
    assert "Invalid include directive" in str(exc_info.value)


def test_parse_invalid_define_format():
    """Test that invalid define format raises ValueError."""
    # Arrange
    source_line = SourceLine(
        line="#define INCOMPLETE",
        source_line_number=1,
        source_file_name="test.asm",
    )

    # Act & Assert
    with pytest.raises(ValueError) as exc_info:
        PreprocessorDirective.parse_line(source_line)
    assert "Invalid define directive" in str(exc_info.value)


def test_preprocessor_simple_define():
    """Test that preprocessor correctly substitutes defined values."""
    # Arrange
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm")
    try:
        temp_file.write("#define VALUE 42\n")
        temp_file.write("load VALUE r1\n")
        temp_file.close()

        file_path = Path(temp_file.name)
        source_lines = [
            SourceLine(line="#define VALUE 42", source_line_number=1, source_file_name=file_path),
            SourceLine(line="load VALUE r1", source_line_number=2, source_file_name=file_path),
        ]
        preprocessor = Preprocessor(source_lines, file_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        assert len(result) == 2
        # The define should be replaced in the second line
        assert "load 42 r1" in result[1].line
    finally:
        os.unlink(temp_file.name)


def test_preprocessor_multiple_defines():
    """Test that preprocessor handles multiple defines correctly."""
    # Arrange
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm")
    try:
        temp_file.write("#define A 10\n")
        temp_file.write("#define B 20\n")
        temp_file.write("add A B r1\n")
        temp_file.close()

        file_path = Path(temp_file.name)
        source_lines = [
            SourceLine(line="#define A 10", source_line_number=1, source_file_name=file_path),
            SourceLine(line="#define B 20", source_line_number=2, source_file_name=file_path),
            SourceLine(line="add A B r1", source_line_number=3, source_file_name=file_path),
        ]
        preprocessor = Preprocessor(source_lines, file_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        assert len(result) == 3
        # Both defines should be replaced
        assert "add 10 20 r1" in result[2].line
    finally:
        os.unlink(temp_file.name)


def test_preprocessor_include_file():
    """Test that preprocessor correctly includes external files."""
    # Arrange
    # Create include file
    include_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm", dir="/tmp")
    try:
        include_file.write("load 1 r1\n")
        include_file.write("add r1 r2 r3\n")
        include_file.close()
        include_path = Path(include_file.name)

        # Create main file
        main_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm", dir="/tmp")
        main_file.write(f'#include "{include_path.name}"\n')
        main_file.write("halt\n")
        main_file.close()
        main_path = Path(main_file.name)

        source_lines = [
            SourceLine(line=f'#include "{include_path.name}"', source_line_number=1, source_file_name=main_path),
            SourceLine(line="halt", source_line_number=2, source_file_name=main_path),
        ]
        preprocessor = Preprocessor(source_lines, main_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        # Should have original 2 lines + 2 lines from include
        assert len(result) == 4
        assert "load 1 r1" in result[2].line
        assert "add r1 r2 r3" in result[3].line
    finally:
        os.unlink(include_file.name)
        os.unlink(main_file.name)


def test_preprocessor_prevent_duplicate_include():
    """Test that preprocessor prevents including the same file twice."""
    # Arrange
    # Create a file that includes itself
    self_include = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm", dir="/tmp")
    try:
        self_include.write(f'#include "{Path(self_include.name).name}"\n')
        self_include.write("nop\n")
        self_include.close()
        file_path = Path(self_include.name)

        source_lines = [
            SourceLine(line=f'#include "{file_path.name}"', source_line_number=1, source_file_name=file_path),
            SourceLine(line="nop", source_line_number=2, source_file_name=file_path),
        ]
        preprocessor = Preprocessor(source_lines, file_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        # The file should not be included recursively
        # Original 2 lines should remain
        assert len(result) == 2
    finally:
        os.unlink(self_include.name)


def test_preprocessor_empty_file():
    """Test that preprocessor handles empty files correctly."""
    # Arrange
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm")
    try:
        temp_file.close()
        file_path = Path(temp_file.name)
        source_lines = []
        preprocessor = Preprocessor(source_lines, file_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        assert len(result) == 0
    finally:
        os.unlink(temp_file.name)


def test_preprocessor_define_with_hex_value():
    """Test that preprocessor handles hex values in defines."""
    # Arrange
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".asm")
    try:
        temp_file.write("#define ADDR 0x1000\n")
        temp_file.write("load ADDR r1\n")
        temp_file.close()

        file_path = Path(temp_file.name)
        source_lines = [
            SourceLine(line="#define ADDR 0x1000", source_line_number=1, source_file_name=file_path),
            SourceLine(line="load ADDR r1", source_line_number=2, source_file_name=file_path),
        ]
        preprocessor = Preprocessor(source_lines, file_path)

        # Act
        result = preprocessor.preprocess()

        # Assert
        assert len(result) == 2
        assert "load 0x1000 r1" in result[1].line
    finally:
        os.unlink(temp_file.name)

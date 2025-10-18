import tempfile
import os
import pytest
from asmpy.assembler import Assembler
from asmpy.models.data_types import SourceLine, Number


def test_assembler_simple_program():
    """Test assembling a simple program with basic instructions."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="add r1 r2 r3", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=4, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        assert os.path.exists(temp_file.name)
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        assert len(lines) == 3  # load, add, halt
    finally:
        os.unlink(temp_file.name)


def test_assembler_with_labels():
    """Test assembling program with labels and jumps."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="Start:", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="jump Start", source_line_number=4, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        assert os.path.exists(temp_file.name)
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        assert len(lines) == 2  # load, jump
    finally:
        os.unlink(temp_file.name)


def test_assembler_with_comments():
    """Test that assembler correctly removes comment-only lines."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="; This is a comment", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="; Another comment", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=5, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        # Comments should be removed, only load and halt remain
        assert len(lines) == 2
    finally:
        os.unlink(temp_file.name)


def test_assembler_with_multiple_directives():
    """Test that assembler correctly reorders lines based on directives."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line=".data", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line=".dw 42", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line=".code", source_line_number=5, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=6, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        assert os.path.exists(temp_file.name)
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        # Code section first (load, halt), then data section (.dw)
        assert len(lines) == 3
    finally:
        os.unlink(temp_file.name)


def test_assembler_duplicate_label_error():
    """Test that assembler raises error for duplicate labels."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="Start:", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="Start:", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=5, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act & Assert
        with pytest.raises(ValueError) as exc_info:
            assembler.assemble()
        assert "already defined" in str(exc_info.value)
    finally:
        os.unlink(temp_file.name)


def test_assembler_label_without_instruction():
    """Test that assembler raises error for label with no instruction."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="End:", source_line_number=3, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act & Assert
        with pytest.raises(ValueError) as exc_info:
            assembler.assemble()
        assert "has no instruction to point to" in str(exc_info.value)
    finally:
        os.unlink(temp_file.name)


def test_assembler_with_offset():
    """Test that assembler correctly applies offset to label addresses."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="Start:", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="jump Start", source_line_number=4, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        offset = Number("0x1000")
        assembler = Assembler(source_lines, temp_file.name, offset_address=offset)

        # Act
        assembler.assemble()

        # Assert
        assert os.path.exists(temp_file.name)
        # Label address should be offset + 0 (first instruction)
        assert assembler._label_address_mappings[list(assembler._label_address_mappings.keys())[0]] == 0x1000
    finally:
        os.unlink(temp_file.name)


def test_assembler_with_header():
    """Test that assembler adds header instructions when requested."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="Main:", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line="load 1 r1", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line="Int:", source_line_number=5, source_file_name="test.asm"),
        SourceLine(line="reti", source_line_number=6, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble(add_header=True)

        # Assert
        assert os.path.exists(temp_file.name)
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        # Header adds 3 instructions: jump Main, jump Int, .dw line_count
        # Total: 3 header + 2 Main + 1 Int = 6 lines
        assert len(lines) == 6
    finally:
        os.unlink(temp_file.name)


def test_assembler_empty_input():
    """Test that assembler handles empty input gracefully."""
    # Arrange
    source_lines = []
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act & Assert
        with pytest.raises(ValueError) as exc_info:
            assembler.assemble()
        assert "No assembly lines" in str(exc_info.value)
    finally:
        os.unlink(temp_file.name)


def test_assembler_pseudo_instruction_expansion():
    """Test that pseudo instructions are expanded correctly."""
    # Arrange
    source_lines = [
        SourceLine(line=".code", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="load32 0x12345678 r1", source_line_number=2, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        # load32 with high bits should expand to 2 instructions (loadhi + load)
        assert len(lines) == 2
    finally:
        os.unlink(temp_file.name)


def test_assembler_data_section():
    """Test that assembler handles .data section with .dw directives."""
    # Arrange
    source_lines = [
        SourceLine(line=".data", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line=".dw 100", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line=".dw 200", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line=".code", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=5, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        with open(temp_file.name, "r") as f:
            lines = f.readlines()
        # halt instruction + 2 .dw values
        assert len(lines) == 3
    finally:
        os.unlink(temp_file.name)


def test_assembler_label_in_data_section():
    """Test that assembler handles labels in data section correctly."""
    # Arrange
    source_lines = [
        SourceLine(line=".data", source_line_number=1, source_file_name="test.asm"),
        SourceLine(line="DataLabel:", source_line_number=2, source_file_name="test.asm"),
        SourceLine(line=".dw 42", source_line_number=3, source_file_name="test.asm"),
        SourceLine(line=".code", source_line_number=4, source_file_name="test.asm"),
        SourceLine(line="load DataLabel r1", source_line_number=5, source_file_name="test.asm"),
        SourceLine(line="halt", source_line_number=6, source_file_name="test.asm"),
    ]
    temp_file = tempfile.NamedTemporaryFile(mode="w", delete=False, suffix=".bin")
    temp_file.close()

    try:
        assembler = Assembler(source_lines, temp_file.name)

        # Act
        assembler.assemble()

        # Assert
        assert os.path.exists(temp_file.name)
        # Verify that label was resolved
        assert len(assembler._label_address_mappings) > 0
    finally:
        os.unlink(temp_file.name)

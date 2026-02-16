from unittest.mock import patch, MagicMock
import pytest
from asmpy.app import main


@patch("asmpy.app.read_input_file", return_value=[])
@patch("asmpy.app.Preprocessor")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_logging(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    MockPreprocessor,
    mock_read_input,
):
    """Test that main correctly configures logging and calls assembler."""
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm",
        output="output.list",
        log_level="DEBUG",
        log_details=True,
        offset="0",
        header=False,
        independent=False,
    )
    mock_preprocessor_instance = MockPreprocessor.return_value
    mock_preprocessor_instance.preprocess.return_value = []
    mock_assembler_instance = MockAssembler.return_value
    mock_assembler_instance.assemble.return_value = None

    # Act
    main()

    # Assert
    mock_configure_logging.assert_called_once_with(
        "DEBUG", MockCustomFormatter.return_value
    )
    MockPreprocessor.assert_called_once()
    MockAssembler.assert_called_once()
    args, kwargs = MockAssembler.call_args
    assert args[0] == []
    assert args[1] == "output.list"
    assert kwargs["offset_address"].value == 0
    assert kwargs["independent"] is False
    mock_assembler_instance.assemble.assert_called_once_with(add_header=False)


@patch("asmpy.app.read_input_file", return_value=[])
@patch("asmpy.app.Preprocessor")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_assembler_exception_exits(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    MockPreprocessor,
    mock_read_input,
):
    """Test that main exits with code 1 when assembler raises an exception."""
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm",
        output="output.list",
        log_level="DEBUG",
        log_details=True,
        offset="0",
        header=False,
        independent=False,
    )
    mock_preprocessor_instance = MockPreprocessor.return_value
    mock_preprocessor_instance.preprocess.return_value = []
    mock_assembler_instance = MockAssembler.return_value
    mock_assembler_instance.assemble.side_effect = Exception("Test Exception")

    # Act & Assert
    with pytest.raises(SystemExit) as exc_info:
        main()

    assert exc_info.value.code == 1
    mock_configure_logging.assert_called_once_with(
        "DEBUG", MockCustomFormatter.return_value
    )
    mock_assembler_instance.assemble.assert_called_once()


@patch("asmpy.app.read_input_file", return_value=[])
@patch("asmpy.app.Preprocessor")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_preprocessor_exception_exits(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    MockPreprocessor,
    mock_read_input,
):
    """Test that main exits with code 1 when preprocessor raises an exception."""
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm",
        output="output.list",
        log_level="DEBUG",
        log_details=True,
        offset="0",
        header=False,
        independent=False,
    )
    mock_preprocessor_instance = MockPreprocessor.return_value
    mock_preprocessor_instance.preprocess.side_effect = Exception("Preprocessor Error")

    # Act & Assert
    with pytest.raises(SystemExit) as exc_info:
        main()

    assert exc_info.value.code == 1
    mock_configure_logging.assert_called_once()
    mock_preprocessor_instance.preprocess.assert_called_once()
    MockAssembler.assert_not_called()


@patch("asmpy.app.read_input_file", return_value=[])
@patch("asmpy.app.Preprocessor")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_invalid_offset_exits(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    MockPreprocessor,
    mock_read_input,
):
    """Test that main exits with code 1 when offset is invalid."""
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm",
        output="output.list",
        log_level="DEBUG",
        log_details=True,
        offset="invalid",
        header=False,
        independent=False,
    )
    mock_preprocessor_instance = MockPreprocessor.return_value
    mock_preprocessor_instance.preprocess.return_value = []

    # Act & Assert
    with pytest.raises(SystemExit) as exc_info:
        main()

    assert exc_info.value.code == 1
    mock_configure_logging.assert_called_once()
    MockAssembler.assert_not_called()


@patch("asmpy.app.read_input_file", return_value=[])
@patch("asmpy.app.Preprocessor")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_independent_ignores_invalid_offset(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    MockPreprocessor,
    mock_read_input,
):
    """Test that independent mode ignores invalid --offset values."""
    mock_parse_args.return_value = MagicMock(
        file="input.asm",
        output="output.list",
        log_level="DEBUG",
        log_details=True,
        offset="invalid",
        header=False,
        independent=True,
    )
    mock_preprocessor_instance = MockPreprocessor.return_value
    mock_preprocessor_instance.preprocess.return_value = []
    mock_assembler_instance = MockAssembler.return_value
    mock_assembler_instance.assemble.return_value = None

    main()

    MockAssembler.assert_called_once()
    args, kwargs = MockAssembler.call_args
    assert args[0] == []
    assert args[1] == "output.list"
    assert kwargs["offset_address"].value == 0
    assert kwargs["independent"] is True
    mock_assembler_instance.assemble.assert_called_once_with(add_header=False)

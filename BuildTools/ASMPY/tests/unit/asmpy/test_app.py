from unittest.mock import patch, MagicMock
from asmpy.app import main


@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_logging(
    MockAssembler, MockCustomFormatter, mock_configure_logging, mock_parse_args
):
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm", output="output.list", log_level="DEBUG", log_details=True
    )
    mock_assembler_instance = MockAssembler.return_value
    mock_assembler_instance.assemble.return_value = None

    # Act
    main()

    # Assert
    mock_configure_logging.assert_called_once_with(
        "DEBUG", MockCustomFormatter.return_value
    )
    MockAssembler.assert_called_with("input.asm", "output.list")
    mock_assembler_instance.assemble.assert_called_once()


@patch("asmpy.app.sys.exit")
@patch("asmpy.app.parse_args")
@patch("asmpy.app.configure_logging")
@patch("asmpy.app.CustomFormatter")
@patch("asmpy.app.Assembler")
def test_main_sys_exit_called(
    MockAssembler,
    MockCustomFormatter,
    mock_configure_logging,
    mock_parse_args,
    mock_sys_exit,
):
    # Arrange
    mock_parse_args.return_value = MagicMock(
        file="input.asm", output="output.list", log_level="DEBUG", log_details=True
    )
    mock_assembler_instance = MockAssembler.return_value
    mock_assembler_instance.assemble.side_effect = Exception("Test Exception")

    # Act
    main()

    # Assert
    mock_configure_logging.assert_called_once_with(
        "DEBUG", MockCustomFormatter.return_value
    )
    MockAssembler.assert_called_with("input.asm", "output.list")
    mock_assembler_instance.assemble.assert_called_once()
    mock_sys_exit.assert_called_once_with(1)

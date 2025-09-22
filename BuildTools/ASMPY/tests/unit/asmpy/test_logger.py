import logging
import sys
from asmpy.logger import CustomFormatter, configure_logging


def test_custom_formatter_basic_format():
    # Arrange
    formatter = CustomFormatter(log_details=False)
    record = logging.LogRecord(
        name="test",
        level=logging.INFO,
        pathname=__file__,
        lineno=10,
        msg="Test message",
        args=(),
        exc_info=None,
    )

    # Act
    formatted_message = formatter.format(record)

    # Assert
    assert "INFO     - Test message" in formatted_message


def test_custom_formatter_detailed_format():
    # Arrange
    formatter = CustomFormatter(log_details=True)
    record = logging.LogRecord(
        name="test",
        level=logging.INFO,
        pathname=__file__,
        lineno=10,
        msg="Test message",
        args=(),
        exc_info=None,
    )

    # Act
    formatted_message = formatter.format(record)

    # Assert
    assert "INFO     - Test message" in formatted_message
    assert "test_logger.py:10" in formatted_message


def test_custom_formatter_color():
    # Arrange
    formatter = CustomFormatter(log_details=False)
    record = logging.LogRecord(
        name="test",
        level=logging.WARNING,
        pathname=__file__,
        lineno=10,
        msg="Test message",
        args=(),
        exc_info=None,
    )

    # Act
    formatted_message = formatter.format(record)

    # Assert
    assert "\x1b[33;20m" in formatted_message
    assert "\x1b[0m" in formatted_message


def test_configure_logging():
    # Arrange
    formatter = logging.Formatter()

    # Act
    configure_logging("debug", formatter)
    root_logger = logging.getLogger()

    # Assert
    assert root_logger.level == logging.DEBUG
    assert len(root_logger.handlers) == 1
    assert root_logger.handlers[0].formatter == formatter
    assert (
        root_logger.handlers[0].stream
        == logging.StreamHandler(stream=sys.stdout).stream
    )

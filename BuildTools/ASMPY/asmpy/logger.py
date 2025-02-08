import logging
import sys


class CustomFormatter(logging.Formatter):
    """Custom logging formatter with colors and option for log details."""

    grey = "\x1b[37;20m"
    white = "\x1b[38;20m"
    yellow = "\x1b[33;20m"
    red = "\x1b[31;20m"
    bold_red = "\x1b[31;1m"
    reset = "\x1b[0m"

    format_detailed = "%(asctime)s - %(name)s - %(levelname)-8s - %(message)s (%(filename)s:%(lineno)d)"
    format_basic = "%(levelname)-8s - %(message)s"

    def __init__(self, log_details: bool = False) -> None:
        """Initialize the formatter with either detailed or basic formatting."""
        super().__init__()
        self.log_format = self.format_detailed if log_details else self.format_basic
        self._FORMATS = {
            logging.DEBUG: self.grey + self.log_format + self.reset,
            logging.INFO: self.white + self.log_format + self.reset,
            logging.WARNING: self.yellow + self.log_format + self.reset,
            logging.ERROR: self.red + self.log_format + self.reset,
            logging.CRITICAL: self.bold_red + self.log_format + self.reset,
        }

    def format(self, record: logging.LogRecord) -> str:
        log_fmt = self._FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


def configure_logging(log_level: str, formatter: logging.Formatter) -> None:
    """Configure the root logger."""

    stream_handler = logging.StreamHandler(stream=sys.stdout)
    stream_handler.setFormatter(formatter)

    root_logger = logging.getLogger()
    root_logger.handlers.clear()

    root_logger.setLevel(log_level.upper())
    root_logger.addHandler(stream_handler)

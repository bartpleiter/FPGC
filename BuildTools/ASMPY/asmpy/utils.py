import argparse


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Assembler for B32P2.")
    parser.add_argument("file", help="The asm file to assemble")
    parser.add_argument("output", help="The assembled output file")
    parser.add_argument(
        "-l",
        "--log-level",
        default="info",
        choices=["debug", "info", "warning", "error", "critical"],
        help="Set the log level",
    )
    parser.add_argument(
        "-d",
        "--log-details",
        action="store_true",
        help="Enable detailed logging with extra details like line numbers and timestamps",
    )
    return parser.parse_args()

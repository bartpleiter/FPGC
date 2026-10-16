"""Helpers shared by all fpgc commands."""

import subprocess
import sys
from pathlib import Path


def run(*cmd: str | Path) -> None:
    """Print and run a command. If it fails, stop with the same exit code."""
    print("+", *cmd, flush=True)
    exit_code = subprocess.call(cmd)
    if exit_code != 0:
        sys.exit(exit_code)

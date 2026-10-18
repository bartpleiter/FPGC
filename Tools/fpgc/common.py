"""Helpers shared by all fpgc commands."""

import subprocess
import sys
from pathlib import Path


def run(
    *cmd: str | Path,
    cwd: Path | None = None,
    log: Path | None = None,
    check: bool = True,
) -> None:
    """Print and run a command. If it fails, stop with the same exit code.

    With `log`, the command's output goes to that file instead of the terminal.
    With `check=False`, a failure doesn't stop the cli.
    """
    print("+", *cmd, flush=True)
    if log is None:
        exit_code = subprocess.call(cmd, cwd=cwd)
    else:
        with log.open("w") as f:
            exit_code = subprocess.call(
                cmd, cwd=cwd, stdout=f, stderr=subprocess.STDOUT
            )
    if exit_code != 0 and check:
        if log is not None:
            print(f"failed, see {log}", file=sys.stderr)
        sys.exit(exit_code)

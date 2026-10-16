"""Main entry point for the FPGC cli."""

import shutil
from pathlib import Path

import click

from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.docs import docs


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def main() -> None:
    """FPGC project tools. Run `fpgc <group> -h` for details."""
    if not Path("Tools/fpgc").is_dir():
        raise click.UsageError("run fpgc from the root of the FPGC repository")


main.add_command(docs)


@main.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
def test(pattern: str) -> None:
    """Run all tests."""
    run("pytest", "-k", pattern)


@main.command()
def check() -> None:
    """Check formatting, lint and types."""
    run("ruff", "format", "--check")
    run("ruff", "check")
    run("mypy", "Tools")
    # TODO: Add run("pytest") here once there are tests


@main.command()
def clean() -> None:
    """Delete all build output."""
    shutil.rmtree(CONFIG.build_dir, ignore_errors=True)

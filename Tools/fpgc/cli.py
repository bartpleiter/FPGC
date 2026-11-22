"""Main entry point for the FPGC cli."""

import shutil
from pathlib import Path

import click

from fpgc.board import board
from fpgc.common import run
from fpgc.config import CONFIG
from fpgc.cpu import cpu
from fpgc.cpu import lint as cpu_lint
from fpgc.io import io
from fpgc.io import lint as io_lint
from fpgc.sdram import lint as sdram_lint
from fpgc.sdram import sdram
from fpgc.system import lint as system_lint
from fpgc.system import system
from fpgc.website import website


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def main() -> None:
    """FPGC project tools. Run `fpgc <group> -h` for details."""
    if not Path("Tools/fpgc").is_dir():
        raise click.UsageError("run fpgc from the root of the FPGC repository")


main.add_command(board)
main.add_command(cpu)
main.add_command(io)
main.add_command(sdram)
main.add_command(system)
main.add_command(website)


@main.command()
@click.option(
    "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
)
def test(pattern: str) -> None:
    """Run all tests."""
    run("pytest", "-k", pattern)


@main.command()
@click.pass_context
def check(ctx: click.Context) -> None:
    """Check formatting, lint, types and the RTL, and run all tests."""
    run("ruff", "format", "--check")
    run("ruff", "check")
    run("mypy", "Tools")
    ctx.invoke(cpu_lint)
    ctx.invoke(io_lint)
    ctx.invoke(sdram_lint)
    ctx.invoke(system_lint)
    run("pytest", "-q")


@main.command()
def clean() -> None:
    """Delete all build output."""
    shutil.rmtree(CONFIG.build_dir, ignore_errors=True)

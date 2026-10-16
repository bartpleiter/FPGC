# Build System and Scripting

How building, testing and tooling work in this repository, and how to add to them without making a mess.

Read this before you add a command, a script, a tool or a test runner. The previous FPGC repository ended up with a 1300-line Makefile, dozens of Bash and Python scripts doing the same jobs in different ways, copy-pasted source lists, and builds that wrote into the source tree. The rules below keep that from happening again.

## Overview

Every project task is a subcommand of one Python command-line tool, `fpgc`:

```bash
uv sync                      # install dependencies and the fpgc command (editable)
source .venv/bin/activate    # put fpgc, mkdocs, pytest, ruff, ... on your PATH
fpgc -h                      # list all commands
fpgc docs serve
```

Without activating the venv, `uv run fpgc ...` works too. Always run `fpgc` from the repository root.

There is no Makefile and there are no shell scripts. The project is a collection of tasks rather than a build graph, and tasks with arguments, defaults and help are simple to write in Python.

## Layout

```
pyproject.toml     the one Python project; declares the fpgc command (and later the toolchain commands)
Tools/fpgc/
  cli.py           the fpgc group: registers the area groups, plus check, test and clean
  config.py        all settings: one pydantic model per area
  common.py        run()
  docs.py          fpgc docs ...
Tests/             system tests for FPGC components (once they exist)
build/             all generated output; gitignored; `fpgc clean` deletes it
```

## Rules

1. **Every project task is an `fpgc` command.** No Makefile, no shell scripts, no standalone Python scripts. Vendored projects with their own Makefile (such as QBE) are built by an `fpgc` command that calls `make -C`.
2. **Commands are `fpgc <area> <action>`**, such as `fpgc docs serve` or `fpgc cpu test`. One module per area, registered with one line in `cli.py`. Only project-wide commands (`check`, `test`, `clean`) sit at the top level.
3. **Every command has a one-line docstring.** That line is the help text.
4. **Toolchain programs (assembler, linker, ...) are separate programs**, each with its own package in `Tools/` and its own `[project.scripts]` entry. `fpgc` starts them with `run()` and never imports them.
5. **External programs are started with `run()`** from `common.py`. It prints the command and stops `fpgc` with the same exit code if the command fails.
6. **All settings and paths live in `config.py`**, in the area's pydantic model. Paths are relative to the repository root. Commands don't hardcode paths, hosts or ports. To change a value for one run, a command has an option whose default comes from the config, such as `--port`.
7. **All output goes to `build/`, each artifact at its own path.** Nothing is written into the source tree, and no two commands share an output file.
8. **Heavy imports (numpy, pillow, serial) go inside the command function**, so `fpgc -h` stays instant.
9. **pytest is the only test runner.** An area's `test` command is a single `run("pytest", ...)`.
10. **The `fpgc` CLI itself has no unit tests.** It's thin glue around other programs, and `fpgc check` (format, lint, mypy) is enough. Toolchain programs and real logic do get tests.

What does **not** exist in this repository:

- a Makefile, host shell scripts (`*.sh`) or a `Scripts/` directory
- Python scripts started as `python3 path/to/script.py`
- test runners other than pytest
- environment-variable or config-file overrides, plugins, a logging framework, or caching. Add one only when there's a real need.

## Adding an area

An area is any part of the project that needs commands, such as the CPU, the bootloader or the blog.

1. **Add a sub-model to `config.py`** for the area's paths and settings, and add it to `Config`:

   ```python
   class CpuConfig(BaseModel):
       tests_dir: Path = Path("Tests/cpu")


   class Config(BaseModel):
       ...
       cpu: CpuConfig = CpuConfig()
   ```

2. **Create `Tools/fpgc/<area>.py`** with a click group and its commands:

   ```python
   """CPU simulation and tests."""

   import click

   from fpgc.common import run
   from fpgc.config import CONFIG

   cfg = CONFIG.cpu


   @click.group()
   def cpu() -> None:
       """CPU simulation and tests."""


   @cpu.command()
   @click.option(
       "-k", "pattern", default="", help="Only run tests matching this pytest pattern."
   )
   def test(pattern: str) -> None:
       """Run the CPU tests."""
       run("pytest", cfg.tests_dir, "-k", pattern)
   ```

3. **Register the group in `cli.py`:** `main.add_command(cpu)`.
4. **Run `fpgc check`.**

A command that needs another one first calls it with `ctx.invoke(...)`, as `fpgc docs deploy` does with `build`.

## Adding a toolchain program

A toolchain program, such as the assembler, is its own command and not part of `fpgc`:

1. Create `Tools/<tool>/` with a `cli.py` that has a `main()`.
2. In `pyproject.toml`, add it to `module-name` (`module-name = ["fpgc", "<tool>"]`) and add `<tool> = "<tool>.cli:main"` to `[project.scripts]`.
3. Run `uv sync`. `<tool>` is now a command in the venv.
4. Put its tests in `Tools/<tool>/tests/`.

## Review checklist

- [ ] `fpgc -h` (or `fpgc <area> -h`) shows the new command with a clear one-line description.
- [ ] No paths, hosts or ports in commands: they come from `config.py`.
- [ ] All output is under `build/`, and `git status` is clean after running the command.
- [ ] External programs are started with `run()`. Toolchain programs are not imported.
- [ ] No new `.sh` files, standalone scripts or custom test runners.
- [ ] `fpgc check` passes.

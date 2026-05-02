"""
Host tests for the BDOS shell front-end.

Builds and runs three small gcc binaries from the modules in
Software/C/bdos/:
  - shell_lex.c   → lexer
  - shell_parse.c → AST parser
  - shell_vars.c  → variable expansion ($VAR, ${VAR}, $?, $#, $0..$9)

Each module is compiled with -DSHELL_HOST_TEST so it pulls
Tests/host/shell_host_stubs.h in place of the BDOS-only "bdos.h".
"""

import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
HOST_DIR = REPO_ROOT / "Tests/host"
BDOS_SRC = REPO_ROOT / "Software/C/bdos"
INCLUDE = BDOS_SRC / "include"

CASES = [
    # (test name, list of source files to compile)
    ("test_shell_lex", ["test_shell_lex.c", "shell_lex.c"]),
    ("test_shell_parse", ["test_shell_parse.c", "shell_parse.c", "shell_lex.c"]),
    ("test_shell_expand", ["test_shell_expand.c", "shell_vars.c"]),
]


def _resolve(name: str) -> Path:
    """Test sources live in Tests/host/, BDOS sources in Software/C/bdos/."""
    p = HOST_DIR / name
    return p if p.exists() else (BDOS_SRC / name)


def _build(tmp_path: Path, name: str, srcs: list[str]) -> Path:
    out = tmp_path / name
    cmd = (
        [
            "gcc",
            "-O0",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-function",
            "-DSHELL_HOST_TEST",
            f"-I{INCLUDE}",
            f"-I{HOST_DIR}",
        ]
        + [str(_resolve(s)) for s in srcs]
        + ["-o", str(out)]
    )
    subprocess.run(cmd, check=True)
    return out


@pytest.mark.parametrize("name,srcs", CASES, ids=[c[0] for c in CASES])
def test_shell_host(tmp_path, name, srcs):
    binary = _build(tmp_path, name, srcs)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, (
        f"{name} failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )

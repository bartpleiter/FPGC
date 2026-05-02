"""
Host tests for libterm.

Builds Tests/host/test_term.c with gcc against the real
Software/C/libfpgc/term/term.c source, runs it, and reports failure
on nonzero exit.
"""

import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
TEST_SRC = REPO_ROOT / "Tests/host/test_term.c"
TERM_SRC = REPO_ROOT / "Software/C/libfpgc/term/term.c"
INCLUDE = REPO_ROOT / "Software/C/libfpgc/include"


@pytest.fixture(scope="session")
def test_binary(tmp_path_factory):
    out = tmp_path_factory.mktemp("term") / "test_term"
    subprocess.run(
        [
            "gcc",
            "-O0",
            "-Wall",
            "-Werror",
            "-Wno-unused-function",
            "-DTERM_HOST_TEST",
            f"-I{INCLUDE}",
            str(TEST_SRC),
            str(TERM_SRC),
            "-o",
            str(out),
        ],
        check=True,
    )
    return out


def test_term_host(test_binary):
    result = subprocess.run([str(test_binary)], capture_output=True, text=True)
    assert result.returncode == 0, (
        f"term host tests failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )

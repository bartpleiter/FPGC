"""
Host tests for libterm v2.

Builds Tests/host/test_term2.c with gcc against the real
Software/C/libfpgc/term/term2.c source, runs it, and reports failure
on nonzero exit.
"""

import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
TEST_SRC  = REPO_ROOT / "Tests/host/test_term2.c"
TERM2_SRC = REPO_ROOT / "Software/C/libfpgc/term/term2.c"
INCLUDE   = REPO_ROOT / "Software/C/libfpgc/include"


@pytest.fixture(scope="session")
def test_binary(tmp_path_factory):
    out = tmp_path_factory.mktemp("term2") / "test_term2"
    subprocess.run([
        "gcc", "-O0", "-Wall", "-Werror",
        "-Wno-unused-function",
        "-DTERM2_HOST_TEST",
        f"-I{INCLUDE}",
        str(TEST_SRC), str(TERM2_SRC),
        "-o", str(out),
    ], check=True)
    return out


def test_term2_host(test_binary):
    result = subprocess.run([str(test_binary)], capture_output=True, text=True)
    assert result.returncode == 0, (
        f"term2 host tests failed:\nstdout:\n{result.stdout}\n"
        f"stderr:\n{result.stderr}"
    )

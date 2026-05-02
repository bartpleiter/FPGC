"""
Host tests for the BDOS /dev/pixpal VFS device.

Builds Tests/host/test_vfs_pixpal.c against the real
Software/C/bdos/vfs.c with VFS_HOST_TEST defined (vfs.c then pulls
Tests/host/vfs_host_stubs.h instead of the BDOS-only "bdos.h").
"""

import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
HOST_DIR = REPO_ROOT / "Tests/host"
BDOS_DIR = REPO_ROOT / "Software/C/bdos"
INCLUDE = BDOS_DIR / "include"

TEST_SRC = HOST_DIR / "test_vfs_pixpal.c"
VFS_SRC = BDOS_DIR / "vfs.c"


@pytest.fixture(scope="session")
def test_binary(tmp_path_factory):
    out = tmp_path_factory.mktemp("vfs") / "test_vfs_pixpal"
    subprocess.run(
        [
            "gcc",
            "-O0",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-function",
            "-DVFS_HOST_TEST",
            f"-I{HOST_DIR}",
            f"-I{INCLUDE}",
            str(TEST_SRC),
            str(VFS_SRC),
            "-o",
            str(out),
        ],
        check=True,
    )
    return out


def test_vfs_pixpal_host(test_binary):
    result = subprocess.run([str(test_binary)], capture_output=True, text=True)
    assert result.returncode == 0, (
        f"vfs pixpal tests failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    )

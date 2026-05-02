"""
Byte-for-byte regression tests: my-cpp output, when fed through cproc/QBE/
asm-link, produces the same final .bin as gcc-cpp output.

For each userBDOS program:
  1. Compile each .c source via gcc-cpp → cproc → QBE → .asm  (gcc-pipeline)
  2. Compile each .c source via my-cpp  → cproc → QBE → .asm  (my-pipeline)
  3. Run asm-link on each set; byte-compare resulting binaries.

The host cpp binary is built once per session via gcc -DCPP_HOST.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

CPROC = REPO_ROOT / "BuildTools/cproc/output/cproc-qbe"
QBE = REPO_ROOT / "BuildTools/QBE/output/qbe"
ASM_LINK_SRC = REPO_ROOT / "Software/C/userBDOS/asm-link.c"
CPP_SRC = REPO_ROOT / "Software/C/userBDOS/cpp.c"
LIBC_INCLUDE = REPO_ROOT / "Software/C/libc/include"
USERLIB_INCLUDE = REPO_ROOT / "Software/C/userlib/include"

USERLIB_SOURCES = [
    "Software/ASM/crt0/crt0_ubdos.asm",
    "Software/C/libc/string/string.c",
    "Software/C/libc/stdlib/stdlib.c",
    "Software/C/libc/stdlib/malloc.c",
    "Software/C/libc/ctype/ctype.c",
    "Software/C/libc/stdio/stdio.c",
    "Software/C/userlib/src/syscall_asm.asm",
    "Software/C/userlib/src/syscall.c",
    "Software/C/userlib/src/io_stubs.c",
    "Software/C/userlib/src/time.c",
    "Software/C/userlib/src/fixedmath.c",
    "Software/C/userlib/src/fixed64_asm.asm",
    "Software/C/userlib/src/fixed64.c",
    "Software/C/userlib/src/plot.c",
    "Software/C/userlib/src/fnp.c",
]

PROGRAMS = [
    "argtest",
    "bench",
    "cmatrix",
    "edit",
    "mbrot",
    "mbrotc",
    "mbroth",
    "snake",
    "tetrisc",
    "tetrish",
    "tree",
]


@pytest.fixture(scope="session")
def tools(tmp_path_factory):
    """Build host asm-link and host cpp binaries once."""
    out_dir = tmp_path_factory.mktemp("cpp-bin")
    asm_link = out_dir / "asm-link"
    cpp = out_dir / "cpp"
    subprocess.run(
        [
            "gcc",
            "-O0",
            "-Wall",
            "-Wno-parentheses",
            "-Wno-unused-function",
            "-DASMLINK_HOST",
            "-o",
            str(asm_link),
            str(ASM_LINK_SRC),
        ],
        check=True,
    )
    subprocess.run(
        [
            "gcc",
            "-O0",
            "-Wall",
            "-Wno-parentheses",
            "-Wno-unused-function",
            "-Wno-unused-variable",
            "-Wno-unused-but-set-variable",
            "-Wno-comment",
            "-DCPP_HOST",
            "-o",
            str(cpp),
            str(CPP_SRC),
        ],
        check=True,
    )
    return asm_link, cpp


def _have_tools():
    return CPROC.is_file() and QBE.is_file()


def _compile_via(src: Path, out_dir: Path, cpp_argv: list) -> Path:
    """Run cpp_argv on src (or copy if .asm) → cproc → qbe → write .asm."""
    base = src.stem
    asm_file = out_dir / f"{base}.asm"
    if src.suffix == ".asm":
        shutil.copy(src, asm_file)
        return asm_file
    cpp = subprocess.run(cpp_argv + [str(src)], capture_output=True, check=True)
    cproc = subprocess.run(
        [str(CPROC), "-t", "b32p3"],
        input=cpp.stdout,
        capture_output=True,
        check=True,
    )
    qbe = subprocess.run(
        [str(QBE)],
        input=cproc.stdout,
        capture_output=True,
        check=True,
    )
    asm_file.write_bytes(qbe.stdout)
    return asm_file


@pytest.mark.skipif(not _have_tools(), reason="cproc/QBE not built")
@pytest.mark.parametrize("program", PROGRAMS)
def test_my_cpp_matches_gcc_cpp(program, tools, tmp_path):
    asm_link, my_cpp = tools

    prog_c = REPO_ROOT / f"Software/C/userBDOS/{program}.c"
    if not prog_c.is_file():
        pytest.skip(f"missing {prog_c}")
    sources = [REPO_ROOT / s for s in USERLIB_SOURCES]
    sources.append(prog_c)
    helper = REPO_ROOT / f"Software/C/userBDOS/{program}_asm.asm"
    if helper.is_file():
        sources.append(helper)

    gcc_dir = tmp_path / "gcc"
    my_dir = tmp_path / "my"
    gcc_dir.mkdir()
    my_dir.mkdir()

    gcc_cpp = [
        "cpp",
        "-nostdinc",
        "-P",
        f"-I{LIBC_INCLUDE}",
        f"-I{USERLIB_INCLUDE}",
    ]
    my_cpp_argv = [
        str(my_cpp),
        f"-I{LIBC_INCLUDE}",
        f"-I{USERLIB_INCLUDE}",
    ]

    gcc_asms = [_compile_via(s, gcc_dir, gcc_cpp) for s in sources]
    my_asms = [_compile_via(s, my_dir, my_cpp_argv) for s in sources]

    gcc_bin = tmp_path / "gcc.bin"
    my_bin = tmp_path / "my.bin"
    subprocess.run(
        [str(asm_link), *map(str, gcc_asms), "-o", str(gcc_bin)],
        check=True,
        capture_output=True,
    )
    subprocess.run(
        [str(asm_link), *map(str, my_asms), "-o", str(my_bin)],
        check=True,
        capture_output=True,
    )

    a = gcc_bin.read_bytes()
    b = my_bin.read_bytes()
    if a != b:
        n = min(len(a), len(b))
        first = next((i for i in range(n) if a[i] != b[i]), n)
        pytest.fail(
            f"{program}: byte mismatch (gcc-cpp={len(a)}, my-cpp={len(b)}, "
            f"first diff at byte {first})"
        )

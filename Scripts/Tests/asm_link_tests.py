"""
Byte-for-byte regression tests: asm-link.c output equals ASMPY linker output.

For each userBDOS program:
  1. Compile all userlib + program .c sources to .asm via cproc/QBE.
  2. Run ASMPY linker (`python -m asmpy.linker -H -i`) to produce asmpy.bin.
  3. Run asm-link host build to produce asmlink.bin.
  4. Assert byte equality.

The host asm-link binary is built once per session via gcc -DASMLINK_HOST.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]

CPROC = REPO_ROOT / "BuildTools/cproc/output/cproc-qbe"
QBE = REPO_ROOT / "BuildTools/QBE/output/qbe"
ASM_LINK_SRC = REPO_ROOT / "Software/C/userBDOS/asm-link.c"
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
def asm_link_bin(tmp_path_factory):
    """Build the host asm-link binary."""
    out_dir = tmp_path_factory.mktemp("asm-link-bin")
    binary = out_dir / "asm-link"
    cmd = [
        "gcc",
        "-O0",
        "-g",
        "-Wall",
        "-Wno-parentheses",
        "-Wno-unused-function",
        "-DASMLINK_HOST",
        "-o",
        str(binary),
        str(ASM_LINK_SRC),
    ]
    subprocess.run(cmd, check=True)
    return binary


def _have_tools():
    return CPROC.is_file() and QBE.is_file()


def _compile_to_asm(c_or_asm_src: Path, out_dir: Path) -> Path:
    """Compile a .c (or copy a .asm) source to a .asm file in out_dir."""
    base = c_or_asm_src.stem
    asm_file = out_dir / f"{base}.asm"
    if c_or_asm_src.suffix == ".asm":
        shutil.copy(c_or_asm_src, asm_file)
        return asm_file
    cpp_cmd = [
        "cpp",
        "-nostdinc",
        "-P",
        f"-I{LIBC_INCLUDE}",
        f"-I{USERLIB_INCLUDE}",
        str(c_or_asm_src),
    ]
    cpp = subprocess.run(cpp_cmd, check=True, capture_output=True)
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


def _list_to_bin(list_path: Path, bin_path: Path):
    """Convert ASMPY .list output (binary text per line, optional comment) to packed .bin."""
    out = bytearray()
    with open(list_path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Strip comment (everything after first whitespace)
            bits = line.split()[0]
            assert len(bits) == 32, f"bad bit line: {line!r}"
            word = int(bits, 2)
            out += word.to_bytes(4, "little")
    bin_path.write_bytes(out)


@pytest.mark.skipif(not _have_tools(), reason="cproc/QBE not built")
@pytest.mark.parametrize("program", PROGRAMS)
def test_byte_for_byte_match(program, asm_link_bin, tmp_path):
    # Resolve sources for this program
    prog_c = REPO_ROOT / f"Software/C/userBDOS/{program}.c"
    if not prog_c.is_file():
        pytest.skip(f"missing {prog_c}")
    sources = [REPO_ROOT / s for s in USERLIB_SOURCES]
    sources.append(prog_c)
    helper = REPO_ROOT / f"Software/C/userBDOS/{program}_asm.asm"
    if helper.is_file():
        sources.append(helper)

    # Step 1: compile all sources to .asm in tmp_path
    asm_files = []
    for src in sources:
        asm_files.append(_compile_to_asm(src, tmp_path))

    # Step 2: ASMPY linker
    asmpy_list = tmp_path / "asmpy.list"
    asmpy_bin = tmp_path / "asmpy.bin"
    subprocess.run(
        [
            "python",
            "-m",
            "asmpy.linker",
            *map(str, asm_files),
            str(asmpy_list),
            "-H",
            "-i",
        ],
        check=True,
        capture_output=True,
    )
    _list_to_bin(asmpy_list, asmpy_bin)

    # Step 3: asm-link
    asmlink_bin = tmp_path / "asmlink.bin"
    subprocess.run(
        [str(asm_link_bin), *map(str, asm_files), "-o", str(asmlink_bin)],
        check=True,
        capture_output=True,
    )

    # Step 4: byte compare
    a = asmpy_bin.read_bytes()
    b = asmlink_bin.read_bytes()
    if a != b:
        # Find first diff
        n = min(len(a), len(b))
        first = next((i for i in range(n) if a[i] != b[i]), n)
        pytest.fail(
            f"{program}: byte mismatch (asmpy={len(a)} bytes, "
            f"asmlink={len(b)} bytes, first diff at byte {first})"
        )

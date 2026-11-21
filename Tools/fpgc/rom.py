"""The boot ROM's contents, as Quartus and the testbenches read them."""

from pathlib import Path

from b32asm.assembler import assemble

ROM_BASE = 0x04000000
ROM_WORDS = 1024  # 4 KiB


def write_rom_hex(source: Path, out: Path) -> Path:
    """Assemble `source` for the ROM, and write it to `out` as one hex word per line,
    padded with 0 to the whole ROM, for `$readmemh`."""
    words = assemble(source.read_text(), ROM_BASE, str(source)).words
    assert len(words) <= ROM_WORDS, f"{source} doesn't fit in the ROM"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(
        "".join(f"{w:08x}\n" for w in words + [0] * (ROM_WORDS - len(words)))
    )
    return out

"""Unit test of cpu_decoder, against the instruction table of b32asm."""

import random
from pathlib import Path

from b32asm.assembler import assemble
from b32asm.isa import (
    ALU_OPS,
    ALU_ZEXT_OPS,
    BRANCH_OPS,
    MULDIV_OPS,
    READ_OPS,
    WRITE_OPS,
    Decoded,
    Opcode,
    decode,
    sext,
)
from b32sim.sim import MASK

from ...unit_bench import UnitBench, Vector

# The instruction types, which are the decoder's one-hot outputs
TYPES = [
    "arith",
    "mult",
    "div",
    "read",
    "write",
    "push",
    "pop",
    "branch",
    "jump",
    "jumpr",
    "halt",
    "reti",
    "ccache",
    "intid",
]

BENCH = UnitBench(
    unit="cpu_decoder",
    folder=Path(__file__).parent,
    inputs=["instr"],
    outputs=[
        *("areg", "breg", "dreg", "alu_op", "branch_op", "branch_signed"),
        *("constant", "alu_use_const", "const_add_pc", "jump_absolute"),
        *("reads_areg", "reads_breg", "writes_dreg", "late"),
        *("mem_byte", "mem_half", "read_sign_extend", "ccache_data_only"),
        *TYPES,
        "illegal",
    ],
)

# The demo has one case per instruction type, plus a few variants and an illegal one.
DEMO = [
    ("add", "add r1 r2 r3"),
    ("add const", "add r1 -5 r3"),
    ("load", "load 0x8000 r4"),
    ("nop", "nop"),
    ("mults", "mults r1 r2 r3"),
    ("divs", "divs r1 7 r3"),
    ("read", "read 8 r2 r1"),
    ("readbu", "readbu -1 r2 r1"),
    ("writeh", "writeh 4 r2 r1"),
    ("push", "push r5"),
    ("pop", "pop r6"),
    ("beq", "beq r1 r2 8"),
    ("bles", "bles r1 r2 -8"),
    ("jump", "jump 64"),
    ("jumpo", "jumpo -8"),
    ("jumpr", "jumpr 4 r3"),
    ("jumpro", "jumpro 4 r3"),
    ("savpc", "savpc r7"),
    ("reti", "reti"),
    ("readintid", "readintid r2"),
    ("ccached", "ccached"),
    ("halt", "halt"),
    ("reserved op", ".dw 0x08000123"),
]

_ALU_CODES = set(ALU_OPS.values())
_MULDIV_NAMES = {code: name for name, code in MULDIV_OPS.items()}
_BRANCH_CODES = {code for code, _ in BRANCH_OPS.values()}  # S is ignored for eq/ne
_READ_NAMES = {code: name for name, code in READ_OPS.items()}
_WRITE_NAMES = {code: name for name, code in WRITE_OPS.items()}
_SINGLE_TYPE = {
    Opcode.SAVPC: "arith",
    Opcode.PUSH: "push",
    Opcode.POP: "pop",
    Opcode.JUMP: "jump",
    Opcode.JUMPR: "jumpr",
    Opcode.HALT: "halt",
    Opcode.RETI: "reti",
    Opcode.CCACHE: "ccache",
    Opcode.INTID: "intid",
}


def instr_type(ins: Decoded) -> str | None:
    """The instruction type, or None if the instruction is illegal."""
    match ins.opcode:
        case Opcode.ARITH | Opcode.ARITHC:
            return "arith" if ins["op"] in _ALU_CODES else None
        case Opcode.ARITHM | Opcode.ARITHMC:
            name = _MULDIV_NAMES.get(ins["op"])
            if name is None:
                return None
            return "mult" if name.startswith("mul") else "div"
        case Opcode.BRANCH:
            return "branch" if ins["cond"] in _BRANCH_CODES else None
        case Opcode.READ:
            return "read" if ins["size"] in _READ_NAMES else None
        case Opcode.WRITE:
            return "write" if ins["size"] in _WRITE_NAMES else None
    return _SINGLE_TYPE[ins.opcode]


def constant(ins: Decoded) -> int:
    """The constant, as the ISS uses it."""
    match ins.opcode:
        case Opcode.ARITHC if ins["op"] in ALU_ZEXT_OPS:
            return ins["c16"]
        case Opcode.ARITHC | Opcode.ARITHMC:
            return sext(ins["c16"], 16) & MASK
        case Opcode.BRANCH | Opcode.JUMPR | Opcode.READ | Opcode.WRITE:
            return sext(ins["c16"], 16) & MASK
        case Opcode.JUMP:
            return sext(ins["c27"], 27) & MASK if ins["o"] else ins["c27"]
        case Opcode.CCACHE:
            return 4  # its target is PC + 4
    return 0


def model(word: int) -> Vector:
    """The decoder's outputs for one instruction word."""
    ins = decode(word)
    opcode = ins.opcode
    areg_low = opcode in (Opcode.ARITHC, Opcode.ARITHMC, Opcode.JUMPR)
    out: Vector = {
        "areg": (word >> 4 if areg_low else word >> 8) & 0xF,
        "breg": (word >> 4) & 0xF,
        "dreg": word & 0xF,
        "alu_op": ALU_OPS["load"] if opcode == Opcode.SAVPC else (word >> 24) & 0xF,
        "branch_op": (word >> 1) & 0x7,
        "branch_signed": word & 1,
    }
    out |= dict.fromkeys(BENCH.outputs[len(out) :], 0)
    t = instr_type(ins)
    if t is None:
        out["illegal"] = 1
        return out

    alu = t in ("arith", "mult", "div")
    alu_b_is_breg = opcode in (Opcode.ARITH, Opcode.ARITHM)
    size = _READ_NAMES.get(ins["size"]) if t == "read" else None
    size = _WRITE_NAMES.get(ins["size"]) if t == "write" else size
    out |= {
        t: 1,
        "constant": constant(ins),
        "alu_use_const": int(alu and not alu_b_is_breg),
        "const_add_pc": int(opcode == Opcode.SAVPC or (t == "jumpr" and ins["o"])),
        "jump_absolute": int(t == "jump" and not ins["o"]),
        "reads_areg": int(
            (alu and opcode != Opcode.SAVPC)
            or t in ("read", "write", "branch", "jumpr")
        ),
        "reads_breg": int((alu and alu_b_is_breg) or t in ("write", "branch", "push")),
        "writes_dreg": int((alu or t in ("read", "pop", "intid")) and out["dreg"] != 0),
        "late": int(t in ("mult", "div", "read", "pop", "intid")),
        "mem_byte": int(size in ("readb", "readbu", "writeb")),
        "mem_half": int(size in ("readh", "readhu", "writeh")),
        "read_sign_extend": int(size in ("readb", "readh")),
        "ccache_data_only": int(t == "ccache" and ins["data_only"]),
    }
    return out


def check(words: list[int], name: str, labels: list[str] | None = None) -> None:
    __tracebackhide__ = True
    BENCH.check([{"instr": w} for w in words], [model(w) for w in words], name, labels)


def test_the_model_is_one_hot() -> None:
    rng = random.Random(1)
    for _ in range(10_000):
        out = model(rng.getrandbits(32))
        assert sum(out[t] for t in TYPES) + out["illegal"] == 1


def test_the_demo_covers_every_type() -> None:
    covered = {instr_type(decode(w)) for w in _demo_words()}
    assert covered == {*TYPES, None}


def test_fields() -> None:
    """Every value of each 4-bit field, for every opcode; the constants' extremes."""
    rng = random.Random(2)
    words = []
    for opcode in Opcode:
        for shift in (0, 4, 8, 24):
            for value in range(16):
                rest = rng.getrandbits(28) & ~(0xF << shift)
                words.append((opcode << 28) | rest | (value << shift))
        words += [(opcode << 28), (opcode << 28) | 0x0FFFFFFF]
    check(words, "fields")


def test_random() -> None:
    rng = random.Random(3)
    words = [
        (opcode << 28) | rng.getrandbits(28) for opcode in Opcode for _ in range(1000)
    ]
    check(words, "random")


def test_demo() -> None:
    words = _demo_words()
    BENCH.demo(
        [{"instr": w} for w in words],
        [model(w) for w in words],
        [label for label, _ in DEMO],
    )


def _demo_words() -> list[int]:
    return [assemble(source).words[0] for _, source in DEMO]

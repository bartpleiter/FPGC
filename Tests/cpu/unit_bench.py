"""Run a unit's testbench on input vectors, and compare its outputs with a model.

The testbench (tb_<unit>.v, built on modules/tb_unit.vh) reads one vector per line:
a label, then hex columns in the order of `inputs`. It applies one vector per clock
cycle, and for each one writes a line of hex columns in the order of `outputs`.

A test computes the expected outputs with its Python model, and calls `check` with
random vectors and edge cases, and `demo` with a short hand-picked list. The demo is
also dumped as a waveform for `fpgc cpu wave`.
"""

from dataclasses import dataclass, field
from pathlib import Path

from .presets import check_preset
from .verilog import BUILD_DIR, RTL_FILES, WAVE_DIR, compile_tb, run_vvp

Vector = dict[str, int]

LABEL_CHARS = 12  # the width of `label` in tb_unit.vh


@dataclass
class UnitBench:
    unit: str  # the module name, such as "cpu_alu"
    folder: Path  # where tb_<unit>.v and <unit>.gtkw are
    inputs: list[str]  # the vector columns after the label, in the testbench's order
    outputs: list[str]  # the output columns, in the testbench's order
    parameters: dict[str, int] = field(default_factory=dict)  # of the testbench

    def run(
        self,
        vectors: list[Vector],
        name: str,
        labels: list[str] | None = None,
        vcd: Path | None = None,
    ) -> list[Vector]:
        """Apply `vectors` and return the outputs, one per vector."""
        out_dir = BUILD_DIR / "modules" / self.unit / name
        tb = self.folder / f"tb_{self.unit}.v"
        vvp = compile_tb(tb, RTL_FILES, out_dir / "tb.vvp", self.parameters)
        labels = labels or [""] * len(vectors)
        vectors_file = out_dir / "vectors.hex"
        outputs_file = out_dir / "outputs.hex"
        vectors_file.write_text(
            "".join(
                " ".join([_label_hex(label), *(f"{v[k]:x}" for k in self.inputs)])
                + "\n"
                for label, v in zip(labels, vectors, strict=True)
            )
        )
        plusargs: dict[str, object] = {"vectors": vectors_file, "outputs": outputs_file}
        if vcd is not None:
            vcd.parent.mkdir(parents=True, exist_ok=True)
            plusargs["vcd"] = vcd
        run_vvp(vvp, **plusargs)

        results = []
        for number, line in enumerate(outputs_file.read_text().splitlines()):
            columns = line.split()
            try:
                results.append(
                    {k: int(c, 16) for k, c in zip(self.outputs, columns, strict=True)}
                )
            except ValueError:
                raise AssertionError(
                    f"vector {number}: outputs {line!r} have x, z or the wrong columns"
                    f" (inputs {_show(vectors[number])})"
                ) from None
        assert len(results) == len(vectors), "the testbench didn't answer every vector"
        return results

    def check(
        self,
        vectors: list[Vector],
        expected: list[Vector],
        name: str,
        labels: list[str] | None = None,
    ) -> None:
        """Run `vectors` and compare each output with `expected`."""
        __tracebackhide__ = True  # makes pytest show the message, not this function
        _compare(vectors, expected, self.run(vectors, name, labels), labels)

    def demo(
        self, vectors: list[Vector], expected: list[Vector], labels: list[str]
    ) -> None:
        """Check the demo vectors, dump them as a waveform and check the preset."""
        __tracebackhide__ = True
        vcd = WAVE_DIR / f"{self.unit}.vcd"
        _compare(vectors, expected, self.run(vectors, "demo", labels, vcd), labels)
        check_preset(self.folder / f"{self.unit}.gtkw", vcd)


def _compare(
    vectors: list[Vector],
    expected: list[Vector],
    got: list[Vector],
    labels: list[str] | None,
) -> None:
    __tracebackhide__ = True
    for number, (vector, want, have) in enumerate(
        zip(vectors, expected, got, strict=True)
    ):
        if want != have:
            label = f" ({labels[number]})" if labels else ""
            raise AssertionError(
                f"vector {number}{label}: inputs {_show(vector)}\n"
                f"  expected {_show(want)}\n"
                f"  got      {_show(have)}"
            )


def _label_hex(label: str) -> str:
    """A label as the hex of its ASCII bytes, padded with spaces."""
    assert len(label) <= LABEL_CHARS, f"label {label!r} is too long"
    return label.ljust(LABEL_CHARS).encode("ascii").hex()


def _show(values: Vector) -> str:
    return " ".join(f"{k}={v:x}" for k, v in values.items())

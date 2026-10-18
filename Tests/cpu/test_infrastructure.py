"""Tests of the test infrastructure itself: the trace comparison and the preset check."""

from pathlib import Path

import pytest

from .lockstep import RtlTrace, compare
from .presets import check_preset

ISS = [
    "1e000000 00000000",
    "1e000004 00000000 r1=00000005",
    "1e000008 ffffffff -> 1e000008",
]


def test_equal_traces_pass() -> None:
    compare(ISS, RtlTrace(list(ISS), [0, 1, 2], "halted"), "rerun")


def test_first_difference_is_reported_with_its_cycle() -> None:
    rtl = RtlTrace(
        [ISS[0], "1e000004 00000000 r1=00000006", ISS[2]], [0, 3, 4], "halted"
    )
    with pytest.raises(AssertionError) as e:
        compare(ISS, rtl, "fpgc cpu wave b32p4 x.asm")
    message = str(e.value)
    assert "differ at instruction 1 (cycle 3)" in message
    assert "RTL  1e000004 00000000 r1=00000006" in message
    assert "  1e000000 00000000" in message  # the instruction before it
    assert "fpgc cpu wave b32p4 x.asm" in message


def test_rtl_stopping_early_is_reported() -> None:
    rtl = RtlTrace(ISS[:1], [0], "timeout: no HALT within 10 cycles")
    with pytest.raises(AssertionError, match="stopped after 1 instructions"):
        compare(ISS, rtl, "rerun")


def test_rtl_retiring_more_is_reported() -> None:
    rtl = RtlTrace([*ISS, ISS[2]], [0, 1, 2, 3], "halted")
    with pytest.raises(AssertionError, match="more than the ISS's 3"):
        compare(ISS, rtl, "rerun")


VCD = """\
$scope module tb $end
$var reg 1 ! clk $end
$scope module dut $end
$var wire 8 " d [7:0] $end
$upscope $end
$upscope $end
$enddefinitions $end
"""


def test_preset_with_known_signals_passes(tmp_path: Path) -> None:
    (tmp_path / "x.vcd").write_text(VCD)
    (tmp_path / "x.gtkw").write_text("[*] comment\n@28\ntb.clk\n@22\ntb.dut.d[7:0]\n")
    check_preset(tmp_path / "x.gtkw", tmp_path / "x.vcd")


def test_preset_with_a_renamed_signal_fails(tmp_path: Path) -> None:
    (tmp_path / "x.vcd").write_text(VCD)
    (tmp_path / "x.gtkw").write_text("@800200\n-Inputs\n@22\ntb.dut.data[7:0]\n")
    with pytest.raises(AssertionError, match=r"tb\.dut\.data"):
        check_preset(tmp_path / "x.gtkw", tmp_path / "x.vcd")

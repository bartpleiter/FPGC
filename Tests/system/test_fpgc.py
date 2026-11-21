"""The whole top level, fpgc, with the boot ROM, in simulation: the boot lines on the
UART, the echo, the three sources of reset, the LEDs, and the pins that aren't used
yet.

The PLL is a model that locks after 20 cycles, the PC's reset takes 50 cycles instead
of 1 ms, and the activity LEDs' tick comes every 256 cycles (see tb_fpgc.v).
"""

from dataclasses import dataclass, field
from pathlib import Path

from fpgc.rom import write_rom_hex

from ..common.presets import check_preset
from ..common.verilog import compile_tb, run_vvp
from .verilog import SYSTEM

FOLDER = Path(__file__).parent
BOOT = FOLDER.parents[1] / "Software/boot/boot.asm"
BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"
BYTE = 1000  # cycles per byte on the line
BOOTED = 50 * BYTE  # the boot lines are sent by then
PINS = {"reset_n": 0, "rts_n": 1, "dtr_n": 2, "locked": 3}


@dataclass
class Run:
    """What a run of the testbench gave."""

    output: bytes
    # cycle, then reset, user LED, rx LED and tx LED, at every change
    changes: list[tuple[int, int, int, int, int]] = field(default_factory=list)
    pins: dict[str, str] = field(default_factory=dict)  # the unused pins at the end

    def resets(self) -> list[int]:
        """The cycles in which a reset started."""
        before = [0] + [c[1] for c in self.changes]
        return [c[0] for c, b in zip(self.changes, before) if c[1] == 1 and b != 1]

    def lit(self, led: int) -> bool:
        """Whether an LED was on at some point: 2 user, 3 rx, 4 tx."""
        return any(c[led] == 1 for c in self.changes)


def run(
    name: str,
    cycles: int,
    uart_input: list[tuple[int, int]] = (),  # type: ignore[assignment]
    events: list[tuple[int, str, int]] = (),  # type: ignore[assignment]
    demo: bool = False,
) -> Run:
    out_dir = SYSTEM.build_dir / "fpgc" / name
    rom = write_rom_hex(BOOT, out_dir / "rom.hex")
    vvp = compile_tb(
        FOLDER / "tb_fpgc.v",
        SYSTEM.rtl_files,
        out_dir / "tb.vvp",
        {"ROM_CONTENTS": f'"{rom}"'},
        SYSTEM.include_dirs,
    )
    input_file = out_dir / "uart_input.txt"
    input_file.write_text("".join(f"{c} {b:02x}\n" for c, b in uart_input))
    events_file = out_dir / "events.txt"
    events_file.write_text("".join(f"{c} {PINS[p]} {v}\n" for c, p, v in events))
    output_file = out_dir / "uart_output.txt"
    trace_file = out_dir / "trace.txt"
    plusargs: dict[str, object] = {
        "cycles": cycles,
        "uart_input": input_file,
        "uart_output": output_file,
        "events": events_file,
        "trace": trace_file,
    }
    vcd = SYSTEM.wave_dir / "fpgc.vcd"
    if demo:
        vcd.parent.mkdir(parents=True, exist_ok=True)
        plusargs["vcd"] = vcd
    run_vvp(vvp, **plusargs)
    if demo:
        check_preset(FOLDER / "fpgc.gtkw", vcd)

    lines = output_file.read_text().split()
    assert "bad" not in lines, "a byte without its stop bit"
    result = Run(bytes(int(b, 16) for b in lines))
    for line in trace_file.read_text().splitlines():
        words = line.split()
        if words[0] == "pin":
            result.pins[words[1]] = words[2]
        else:
            # x, before the first clock edge, counts as -1
            c, reset, user, rx, tx = (int(w) if w != "x" else -1 for w in words)
            result.changes.append((c, reset, user, rx, tx))
    return result


def test_boot_lines_and_leds() -> None:
    r = run("boot", BOOTED)
    assert r.output == BOOT_LINES
    assert r.resets() == [0]
    assert r.lit(4), "the tx LED never lit"
    assert not r.lit(3), "the rx LED lit without input"
    assert not r.lit(2), "the user LED lit"


def test_echo() -> None:
    r = run("echo", BOOTED + 4 * BYTE, uart_input=[(BOOTED, ord("o")), (0, ord("k"))])
    assert r.output == BOOT_LINES + b"ok"
    assert r.lit(3), "the rx LED never lit"


def test_unused_pins() -> None:
    """Every chip that isn't used yet is deselected, and nothing drives a data line."""
    r = run("pins", 100)
    assert r.pins == {
        "uart_tx_at_start": "1",  # the line is high from configuration on
        "led_gpu": "0",
        "led_flash": "0",
        "led_usb": "0",
        "led_eth": "0",
        "SDRAM_CLK": "0",
        "SDRAM_CSn": "1",
        "SDRAM_CKE": "0",
        "SDRAM_DQM": "1111",
        "SDRAM_DQ": "z" * 32,
        "SRAM_CSn": "1",
        "SRAM_WEn": "1",
        "SRAM_OEn": "1",
        "SRAM_DQ": "z" * 8,
        "usb1_cs": "1",
        "usb2_cs": "1",
        "eth_cs": "1",
        "sd_cs": "1",
        "flash1_cs": "1",
        "flash2_cs": "1",
        "flash2_miso": "z",
        "audio_dac_data": "0" * 8,
    }


def test_resets() -> None:
    """Opening and closing the serial port doesn't reset. The PC's reset (RTS without
    DTR), the button, and a PLL that loses its lock each do, and the ROM starts
    again."""
    t = BOOTED
    pc, button, pll = t + 7000, 2 * t + 8000, 3 * t + 9000
    events = [
        # Opening the port: RTS and DTR together, then closing it
        (t, "rts_n", 0),
        (t + 1, "dtr_n", 0),
        (t + 5000, "rts_n", 1),
        (t + 5001, "dtr_n", 1),
        # Too short for a reset
        (t + 6000, "rts_n", 0),
        (t + 6030, "rts_n", 1),
        # The PC's reset
        (pc, "rts_n", 0),
        (pc + 100, "rts_n", 1),
        # The button
        (button, "reset_n", 0),
        (button + 10, "reset_n", 1),
        # The PLL
        (pll, "locked", 0),
        (pll + 10, "locked", 1),
    ]
    r = run("resets", pll + t, events=events)
    resets = r.resets()
    assert len(resets) == 4, f"resets at {resets}"
    assert pc + 50 < resets[1] < pc + 60  # 50 cycles of RTS, and the synchronizer
    assert button < resets[2] < button + 5
    assert pll < resets[3] < pll + 5
    assert r.output == 4 * BOOT_LINES


# The demo boots, and echoes an "!". The boot lines are sent from about 0.5 µs on, and
# the echo comes at about 500 µs.
def test_demo() -> None:
    r = run("demo", BOOTED + 3 * BYTE, uart_input=[(BOOTED, ord("!"))], demo=True)
    assert r.output == BOOT_LINES + b"!"

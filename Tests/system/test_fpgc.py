"""The whole top level, fpgc, with the boot ROM, in simulation: the boot lines on the
UART, loading programs over it into the SDRAM (the chip models) and running them, the
three sources of reset, the LEDs, the SDRAM's initialization, and the pins that aren't
used yet.

The PLL is a model that locks after 20 cycles, the PC's reset takes 50 cycles instead
of 1 ms, and the activity LEDs' tick comes every 256 cycles (see tb_fpgc.v).
"""

from dataclasses import dataclass, field
from pathlib import Path

from b32asm.assembler import assemble
from fpgc.rom import write_rom_hex
from uartload.protocol import load_message, ok_line

from ..common.presets import check_preset
from ..common.verilog import compile_tb, run_vvp
from .verilog import SYSTEM

FOLDER = Path(__file__).parent
BOOT = FOLDER.parents[1] / "Software/boot/boot.asm"
PROGRAMS = FOLDER.parents[1] / "Software/programs"
BOOT_LINES = b"boot: err 00000000 pc 00000000\r\nboot: ready\r\n"
BYTE = 200  # cycles per byte on the line, at 5 Mbaud (see tb_fpgc.v)
BOOTED = 50 * BYTE  # the boot lines are sent by then
PINS = {"reset_n": 0, "rts_n": 1, "dtr_n": 2, "locked": 3}


@dataclass
class Run:
    """What a run of the testbench gave."""

    output: bytes
    # cycle, then reset, user LED, rx LED and tx LED, at every change
    changes: list[tuple[int, int, int, int, int]] = field(default_factory=list)
    pins: dict[str, str] = field(default_factory=dict)  # the unused pins at the end
    sdram_initialized: bool = False  # both chip models have their mode and refreshes

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
        elif words[0] == "sdram":
            result.sdram_initialized = words[2] == "1"
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
    assert r.sdram_initialized, "the SDRAM wasn't initialized"


def sent(message: bytes, at: int) -> list[tuple[int, int]]:
    """The UART input of `message`, from cycle `at` on, without a pause between the
    bytes, as the PC sends it."""
    return [(at if i == 0 else 0, b) for i, b in enumerate(message)]


def test_load() -> None:
    """The hello world is loaded into SDRAM, runs, and returns to the boot ROM."""
    hello = assemble((PROGRAMS / "hello.asm").read_text(), 0, "hello.asm").to_bytes()
    message = load_message(hello)
    r = run("load", BOOTED + (len(message) + 80) * BYTE, sent(message, BOOTED))
    assert r.output == (
        BOOT_LINES + ok_line(hello) + b"\r\nhello, world\r\n" + BOOT_LINES
    )
    assert r.lit(3), "the rx LED never lit"


def test_memtest() -> None:
    """The memory test over 1 KiB (32 lines) instead of 64 MiB, through the caches,
    the SDRAM controller and the chip models."""
    source = (PROGRAMS / "memtest.asm").read_text()
    assert ".equ END 0x04000000" in source
    source = source.replace(".equ END 0x04000000", ".equ END 0x00010400")
    program = assemble(source, 0, "memtest.asm").to_bytes()
    message = load_message(program)
    cycles = BOOTED + len(message) * BYTE + 150_000  # the test takes about 100,000
    r = run("memtest", cycles, sent(message, BOOTED))
    assert r.output == (
        BOOT_LINES
        + ok_line(program)
        + b"\r\n"
        + b"memtest: pass 00000000 done, errors 00000000\r\n"
        + b"memtest: pass 00000001 done, errors 00000000\r\n"
        + b"memtest: pass 00000002 done, errors 00000000\r\n"
        + b"memtest: done, errors 00000000\r\n"
        + BOOT_LINES
    )


def test_unused_pins() -> None:
    """Every chip that isn't used yet is deselected, and nothing drives a data line."""
    r = run("pins", 100)
    assert r.pins == {
        "uart_tx_at_start": "1",  # the line is high from configuration on
        "led_gpu": "0",
        "led_flash": "0",
        "led_usb": "0",
        "led_eth": "0",
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


# The demo boots, and loads and runs the hello world. The boot lines are sent from about
# 0.5 µs on, the program from 500 µs on, and it runs at about 1.5 ms.
def test_demo() -> None:
    hello = assemble((PROGRAMS / "hello.asm").read_text(), 0, "hello.asm").to_bytes()
    message = load_message(hello)
    r = run(
        "demo", BOOTED + (len(message) + 80) * BYTE, sent(message, BOOTED), demo=True
    )
    assert r.output == (
        BOOT_LINES + ok_line(hello) + b"\r\nhello, world\r\n" + BOOT_LINES
    )

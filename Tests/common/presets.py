"""Check that every signal of a GTKWave preset (.gtkw) exists in a waveform dump.

GTKWave silently leaves out a signal it can't find, so a renamed signal would
quietly disappear from a preset without this check.
"""

import re
from pathlib import Path


def preset_signals(gtkw: Path) -> list[str]:
    """The signal names in a preset, without their bit ranges."""
    names = []
    for line in gtkw.read_text().splitlines():
        line = line.strip()
        # Settings, display flags, group names and comments; the rest are signals.
        if not line or line[0] in "[@-*#":
            continue
        if line.startswith("+{"):  # an alias: +{name} signal
            line = line.split("} ", 1)[1]
        names.append(re.sub(r"\[[^\]]*\]$", "", line))
    return names


def vcd_signals(vcd: Path) -> set[str]:
    """The full names (scope.scope.name) of all signals in a VCD file."""
    names = set()
    scopes: list[str] = []
    with vcd.open() as f:
        for line in f:
            tokens = line.split()
            if not tokens:
                continue
            if tokens[0] == "$scope":
                scopes.append(tokens[2])
            elif tokens[0] == "$upscope":
                scopes.pop()
            elif tokens[0] == "$var":
                names.add(".".join([*scopes, tokens[4]]))
            elif tokens[0] == "$enddefinitions":
                break
    return names


def check_preset(gtkw: Path, vcd: Path) -> None:
    """Fail if the preset has no signals, or one that isn't in the dump."""
    signals = preset_signals(gtkw)
    assert signals, f"{gtkw.name} has no signals"
    dumped = vcd_signals(vcd)
    missing = [s for s in signals if s not in dumped]
    assert not missing, f"{gtkw.name}: not in {vcd.name}: {', '.join(missing)}"

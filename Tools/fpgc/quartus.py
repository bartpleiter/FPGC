"""Quartus timing runs. Generate a project, compile it and report every corner.

The project, the SDC and the STA script are generated into
the output folder, so there are no hand-maintained Quartus files.
"""

import re
import statistics
import subprocess
import sys
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

from fpgc.common import run

# Settings that differ from the Quartus defaults, the same as in the full project.
# Quartus doesn't define SYNTHESIS itself, so the macro keeps the simulation-only code
# (`ifndef SYNTHESIS) out of the build. Verilator and Icarus don't define it, so they
# still check it.
QSF = """\
set_global_assignment -name FAMILY "Cyclone IV E"
set_global_assignment -name DEVICE {device}
set_global_assignment -name TOP_LEVEL_ENTITY {top}
set_global_assignment -name MIN_CORE_JUNCTION_TEMP "-40"
set_global_assignment -name MAX_CORE_JUNCTION_TEMP 100
set_global_assignment -name NOMINAL_CORE_SUPPLY_VOLTAGE 1.2V
set_global_assignment -name OPTIMIZATION_MODE BALANCED
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
set_global_assignment -name SDC_FILE {top}.sdc
set_global_assignment -name SEED {seed}
set_global_assignment -name VERILOG_MACRO "SYNTHESIS=1"
"""

# The clock comes from a pin: clk_50, the board clock, when the top level has a PLL
# that makes the CPU clock from it, or clk, which is the CPU clock itself. The other
# pins only feed and observe the design under test, so they aren't timed.
SDC = """\
create_clock -name clk -period {period} [get_ports clk]
create_clock -name clk_50 -period {board_period} [get_ports clk_50]
derive_pll_clocks
derive_clock_uncertainty
set_false_path -from [all_inputs] -to [all_registers]
set_false_path -from [all_registers] -to [all_outputs]
"""

# Writes timing.tsv with, per corner, the setup and hold summary, the number of failing
# endpoints and the worst setup path to each of the 1000 worst endpoints. The 10
# worst paths of each corner are also written in full to worst_<corner>.rpt.
STA_SCRIPT = """\
project_open {top}
create_timing_netlist
read_sdc
update_timing_netlist
set out [open timing.tsv w]
foreach corner [get_available_operating_conditions] {{
    set_operating_conditions $corner
    update_timing_netlist
    foreach check {{setup hold}} {{
        foreach info [get_clock_domain_info -$check] {{
            puts $out "summary\\t$corner\\t$check\\t[lindex $info 1]\\t[lindex $info 2]"
        }}
    }}
    set failing [get_timing_paths -setup -npaths 100000 -nworst 1 -less_than_slack 0]
    puts $out "failing\\t$corner\\t[get_collection_size $failing]"
    foreach_in_collection p [get_timing_paths -setup -npaths 1000 -nworst 1] {{
        set from [get_node_info -name [get_path_info $p -from]]
        set to [get_node_info -name [get_path_info $p -to]]
        puts $out "path\\t$corner\\t[get_path_info $p -slack]\\t$from\\t$to"
    }}
    report_timing -setup -npaths 10 -nworst 1 -detail full_path \\
        -file worst_$corner.rpt
}}
close $out
project_close
"""

CORNER_NAMES = {
    "7_slow_1200mv_100c": "Slow 100C",
    "7_slow_1200mv_-40c": "Slow -40C",
    "MIN_fast_1200mv_-40c": "Fast -40C",
}
WORST_CORNER = "7_slow_1200mv_100c"


def prepare(
    out_dir: Path,
    top: str,
    files: list[Path],
    device: str,
    periods: tuple[float, float],
    seed: int,
    parameters: dict[str, int] | None = None,
) -> None:
    """Write the project, the SDC and the STA script of one compile into `out_dir`.

    `periods` are those of the clk and clk_50 pins. `parameters` overrides
    parameters of the top level.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    qsf = QSF.format(device=device, top=top, seed=seed)
    qsf += "".join(
        f"set_parameter -name {name} {value}\n"
        for name, value in (parameters or {}).items()
    )
    qsf += "".join(
        f"set_global_assignment -name VERILOG_FILE {f.absolute()}\n" for f in files
    )
    (out_dir / f"{top}.qsf").write_text(qsf)
    period, board_period = periods
    (out_dir / f"{top}.sdc").write_text(
        SDC.format(period=period, board_period=board_period)
    )
    (out_dir / "report.tcl").write_text(STA_SCRIPT.format(top=top))


def compile_quietly(quartus_bin: Path, out_dir: Path, top: str) -> bool:
    """Compile a prepared project and report its timing. Returns whether both
    worked; the output is in compile.log and sta.log."""
    steps: list[tuple[list[str | Path], str]] = [
        ([quartus_bin / "quartus_sh", "--flow", "compile", top], "compile.log"),
        ([quartus_bin / "quartus_sta", "-t", "report.tcl"], "sta.log"),
    ]
    for cmd, log in steps:
        with (out_dir / log).open("w") as f:
            if subprocess.call(cmd, cwd=out_dir, stdout=f, stderr=subprocess.STDOUT):
                print(f"failed, see {out_dir / log}", file=sys.stderr)
                return False
    return True


def compile_and_report(quartus_bin: Path, out_dir: Path, top: str) -> None:
    """Compile a prepared project and print its timing summary."""
    run(
        quartus_bin / "quartus_sh",
        "--flow",
        "compile",
        top,
        cwd=out_dir,
        log=out_dir / "compile.log",
    )
    run(
        quartus_bin / "quartus_sta",
        "-t",
        "report.tcl",
        cwd=out_dir,
        log=out_dir / "sta.log",
    )
    print(summary(out_dir, top))


def compile_seeds(quartus_bin: Path, out_dirs: dict[int, Path], top: str) -> None:
    """Compile the prepared projects of several seeds in parallel, and print a summary
    over the seeds."""
    print(f"Compiling {top} with seeds {', '.join(map(str, out_dirs))}", flush=True)
    with ThreadPoolExecutor(max_workers=len(out_dirs)) as pool:
        ok = list(
            pool.map(lambda d: compile_quietly(quartus_bin, d, top), out_dirs.values())
        )
    if not all(ok):
        sys.exit(1)
    print(seeds_summary(out_dirs, top))


def node_name(node: str) -> str:
    """A node name without bus indices, M9K block numbers and LUT suffixes, and with a
    RAM as its instance followed by |RAM, to group paths."""
    node = re.sub(r"\[\d+\]", "", node)
    node = re.sub(r"~\w+", "", node)
    ram = re.match(r"(.*?)\|altsyncram", node)
    return ram.group(1) + "|RAM" if ram else node


@dataclass
class Timing:
    """The timing of one compile, from its timing.tsv."""

    slacks: dict[str, dict[str, float]]  # corner -> setup/hold -> worst slack
    failing: dict[str, int]  # corner -> failing endpoints
    groups: dict[tuple[str, str], float]  # (from, to) -> worst slack at WORST_CORNER
    counts: dict[tuple[str, str], int]  # (from, to) -> endpoints in the 1000 worst

    def worst(self, check: str) -> float:
        """The worst slack of `check` (setup or hold) over every corner."""
        return min(checks[check] for checks in self.slacks.values())


def read_timing(out_dir: Path) -> Timing:
    t = Timing(defaultdict(dict), {}, {}, defaultdict(int))
    for row in (out_dir / "timing.tsv").read_text().splitlines():
        kind, corner, *rest = row.split("\t")
        if kind == "summary":  # one line per clock domain
            check, slack = rest[0], float(rest[1])
            t.slacks[corner][check] = min(t.slacks[corner].get(check, slack), slack)
        elif kind == "failing":
            t.failing[corner] = int(rest[0])
        elif kind == "path" and corner == WORST_CORNER:
            group = (node_name(rest[1]), node_name(rest[2]))
            t.groups[group] = min(t.groups.get(group, float("inf")), float(rest[0]))
            t.counts[group] += 1
    return t


def summary(out_dir: Path, top: str) -> str:
    """The text summary of a finished run: corners, path groups and resources."""
    t = read_timing(out_dir)
    lines = [f"Timing of {top} in {out_dir}:"]
    for corner, name in CORNER_NAMES.items():
        s = t.slacks[corner]
        setup = f"{s['setup']:+.3f} ns" if "setup" in s else "-"
        hold = f"{s['hold']:+.3f} ns" if "hold" in s else "-"
        lines.append(
            f"  {name:10} setup {setup}, hold {hold}, "
            f"{t.failing.get(corner, '?')} failing endpoints"
        )
    lines.append("Worst paths at Slow 100C, grouped (slack, endpoints, from -> to):")
    ranked = sorted(t.groups.items(), key=lambda item: item[1])
    for (src, dst), slack in ranked[:10]:
        lines.append(f"  {slack:+.3f} ns  {t.counts[(src, dst)]:4}  {src} -> {dst}")
    lines.append("Resources: " + resources(out_dir / "output_files", top))
    return "\n".join(lines)


def seeds_summary(out_dirs: dict[int, Path], top: str) -> str:
    """The text summary of the same design compiled with several seeds."""
    timings = {seed: read_timing(d) for seed, d in out_dirs.items()}
    lines = [f"Timing of {top} over {len(timings)} seeds (worst of every corner):"]
    for seed, t in timings.items():
        failing = sum(t.failing.values())
        les = resources(out_dirs[seed] / "output_files", top).split(" LEs")[0]
        lines.append(
            f"  seed {seed:2}  setup {t.worst('setup'):+.3f} ns"
            f"  hold {t.worst('hold'):+.3f} ns  {failing} failing endpoints  {les} LEs"
        )
    setup = [t.worst("setup") for t in timings.values()]
    hold = [t.worst("hold") for t in timings.values()]
    passing = sum(s >= 0 and h >= 0 for s, h in zip(setup, hold, strict=True))
    lines.append(
        f"  setup min {min(setup):+.3f}, mean {statistics.mean(setup):+.3f}, "
        f"median {statistics.median(setup):+.3f} ns; hold min {min(hold):+.3f} ns; "
        f"{passing}/{len(setup)} seeds pass"
    )
    lines.append(
        "Worst path groups at Slow 100C (worst slack over the seeds, the seeds that"
        " have it in their 1000 worst, mean slack over those, from -> to):"
    )
    groups: dict[tuple[str, str], list[float]] = defaultdict(list)
    for t in timings.values():
        for group, slack in t.groups.items():
            groups[group].append(slack)
    ranked = sorted(groups.items(), key=lambda item: min(item[1]))
    for (src, dst), slacks in ranked[:15]:
        lines.append(
            f"  {min(slacks):+.3f} ns  {len(slacks):2}  {statistics.mean(slacks):+.3f} ns"
            f"  {src} -> {dst}"
        )
    first = next(iter(out_dirs.values()))
    lines.append("Resources: " + resources(first / "output_files", top))
    return "\n".join(lines)


def resources(output_files: Path, top: str) -> str:
    """Logic elements, registers, memory bits, M9Ks and multipliers of the fit."""
    wanted = {
        "Total logic elements": "LEs",
        "Total registers": "registers",
        "Total memory bits": "memory bits",
        "Embedded Multiplier 9-bit elements": "9-bit multipliers",
    }
    found = []
    for line in (output_files / f"{top}.fit.summary").read_text().splitlines():
        key, _, value = line.partition(" : ")
        if key.strip() in wanted:
            found.append(f"{value.split(' / ')[0].strip()} {wanted[key.strip()]}")
    report = (output_files / f"{top}.fit.rpt").read_text(errors="replace")
    m9ks = re.search(r"^; M9Ks\s+; (\d+) /", report, re.MULTILINE)
    if m9ks:
        found.insert(-1, f"{m9ks.group(1)} M9Ks")
    return ", ".join(found)

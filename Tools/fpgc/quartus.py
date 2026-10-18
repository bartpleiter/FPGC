"""Quartus timing runs: generate a project, compile it and report every corner.

The project, the SDC and the STA script are generated into
the output folder, so there are no hand-maintained Quartus files.
"""

import re
from collections import defaultdict
from pathlib import Path

from fpgc.common import run

# Settings that differ from the Quartus defaults, the same as in the full project.
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
"""

# The clock goes straight to the pin (no PLL). The other pins only feed and observe
# the design under test, so they aren't timed.
SDC = """\
create_clock -name clk -period {period} [get_ports clk]
derive_clock_uncertainty
set_false_path -from [all_inputs] -to [all_registers]
set_false_path -from [all_registers] -to [all_outputs]
"""

# Writes timing.tsv: per corner the setup and hold summary, the number of failing
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


def compile_and_report(
    quartus_bin: Path,
    out_dir: Path,
    top: str,
    files: list[Path],
    device: str,
    period_ns: float,
    seed: int,
) -> None:
    """Compile `files` with top level `top` in `out_dir` and print the timing summary."""
    out_dir.mkdir(parents=True, exist_ok=True)
    qsf = QSF.format(device=device, top=top, seed=seed)
    qsf += "".join(
        f"set_global_assignment -name VERILOG_FILE {f.absolute()}\n" for f in files
    )
    (out_dir / f"{top}.qsf").write_text(qsf)
    (out_dir / f"{top}.sdc").write_text(SDC.format(period=period_ns))
    (out_dir / "report.tcl").write_text(STA_SCRIPT.format(top=top))

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


def group_name(node: str) -> str:
    """A node name without bus indices and M9K block numbers, to group paths."""
    node = re.sub(r"\[\d+\]", "", node)
    return re.sub(r"ram_block\d+a\d+", "ram_block", node)


def summary(out_dir: Path, top: str) -> str:
    """The text summary of a finished run: corners, path groups and resources."""
    lines = []
    slacks: dict[str, dict[str, str]] = defaultdict(dict)
    failing: dict[str, str] = {}
    groups: dict[tuple[str, str], list[float]] = defaultdict(list)
    worst_corner = "7_slow_1200mv_100c"
    for row in (out_dir / "timing.tsv").read_text().splitlines():
        kind, corner, *rest = row.split("\t")
        if kind == "summary":
            check, slack, tns = rest
            slacks[corner][check] = f"{float(slack):+.3f} ns (TNS {float(tns):.3f})"
        elif kind == "failing":
            failing[corner] = rest[0]
        elif kind == "path" and corner == worst_corner:
            slack, src, dst = rest
            groups[(group_name(src), group_name(dst))].append(float(slack))

    lines.append(f"Timing of {top} in {out_dir}:")
    for corner, name in CORNER_NAMES.items():
        s = slacks[corner]
        lines.append(
            f"  {name:10} setup {s.get('setup', '-')}, hold {s.get('hold', '-')}, "
            f"{failing.get(corner, '?')} failing endpoints"
        )
    lines.append("Worst paths at Slow 100C, grouped (slack, endpoints, from -> to):")
    ranked = sorted(groups.items(), key=lambda item: min(item[1]))
    for (src, dst), group in ranked[:10]:
        lines.append(f"  {min(group):+.3f} ns  {len(group):4}  {src} -> {dst}")
    lines.append(
        "Resources: " + resources(out_dir / "output_files" / f"{top}.fit.summary")
    )
    return "\n".join(lines)


def resources(fit_summary: Path) -> str:
    """Logic elements, registers, memory bits and multipliers from the fit summary."""
    wanted = {
        "Total logic elements": "LEs",
        "Total registers": "registers",
        "Total memory bits": "memory bits",
        "Embedded Multiplier 9-bit elements": "9-bit multipliers",
    }
    found = []
    for line in fit_summary.read_text().splitlines():
        key, _, value = line.partition(" : ")
        if key.strip() in wanted:
            found.append(f"{value.split(' / ')[0].strip()} {wanted[key.strip()]}")
    return ", ".join(found)

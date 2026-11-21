"""The RTL and test folders of the top level: the system's own files, with the CPU's,
the I/O devices' and a simulation model of the PLL."""

from fpgc.config import CONFIG

from ..common.verilog import REPO, Area

CFG = CONFIG.system

SYSTEM = Area(
    rtl_files=tuple(
        REPO / f
        for f in [
            *CFG.rtl_files,
            CFG.pll_model,
            *CONFIG.cpu.rtl_files,
            *CONFIG.io.rtl_files,
        ]
    ),
    include_dirs=(REPO / CFG.tests_dir, REPO / CONFIG.io.tests_dir),
    build_dir=REPO / CFG.build_dir,
    wave_dir=REPO / CFG.wave_dir,
)

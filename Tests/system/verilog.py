"""The RTL and test folders of the top level: the system's own files, with the CPU's,
the I/O devices' and the SDRAM controller's, a simulation model of the PLL, and the
model of the SDRAM chips."""

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
            *CONFIG.sdram.rtl_files,
            CONFIG.sdram.chip_model,
        ]
    ),
    include_dirs=(REPO / CFG.tests_dir, REPO / CONFIG.io.tests_dir),
    build_dir=REPO / CFG.build_dir,
    wave_dir=REPO / CFG.wave_dir,
)

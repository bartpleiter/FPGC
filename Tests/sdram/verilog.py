"""The RTL and test folders of the SDRAM controller, with the model of the chip."""

from fpgc.config import CONFIG

from ..common.verilog import REPO, Area

CFG = CONFIG.sdram

SDRAM = Area(
    rtl_files=tuple(REPO / f for f in [*CFG.rtl_files, CFG.chip_model]),
    include_dirs=(REPO / CFG.tests_dir,),
    build_dir=REPO / CFG.build_dir,
    wave_dir=REPO / CFG.wave_dir,
)

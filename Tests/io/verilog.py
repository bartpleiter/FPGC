"""The RTL and test folders of the I/O devices."""

from fpgc.config import CONFIG

from ..common.verilog import REPO, Area

CFG = CONFIG.io

IO = Area(
    rtl_files=tuple(REPO / f for f in CFG.rtl_files),
    include_dirs=(REPO / CFG.tests_dir,),
    build_dir=REPO / CFG.build_dir,
    wave_dir=REPO / CFG.wave_dir,
)

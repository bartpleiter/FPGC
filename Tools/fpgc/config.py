"""All settings of the fpgc commands. Paths are relative to the repository root."""

from pathlib import Path

from pydantic import BaseModel


class WebsiteConfig(BaseModel):
    source_dir: Path = Path("Website")
    theme_dir: Path = Path("Website/themes/hextra")
    site_dir: Path = Path("build/website")
    port: int = 8088
    deploy_target: str = "192.168.0.240:/home/bart/NFS/fpgc/"


class CpuConfig(BaseModel):
    top: str = "b32p4"
    # The RTL files used in simulation, linting and Quartus
    rtl_files: list[Path] = [
        Path("Hardware/cpu/b32p4.v"),
        Path("Hardware/cpu/cpu_hazard.v"),
        Path("Hardware/cpu/cpu_memory_map.v"),
        Path("Hardware/cpu/if/cpu_if.v"),
        Path("Hardware/cpu/id/cpu_id.v"),
        Path("Hardware/cpu/id/cpu_decoder.v"),
        Path("Hardware/cpu/id/cpu_regfile.v"),
        Path("Hardware/cpu/id/cpu_forward.v"),
        Path("Hardware/cpu/ex/cpu_ex.v"),
        Path("Hardware/cpu/ex/cpu_alu.v"),
        Path("Hardware/cpu/ex/cpu_branch.v"),
        Path("Hardware/cpu/ex/cpu_multiplier.v"),
        Path("Hardware/cpu/ex/cpu_divider.v"),
        Path("Hardware/cpu/mem/cpu_mem.v"),
        Path("Hardware/cpu/mem/cpu_read_align.v"),
        Path("Hardware/cpu/mem/cpu_write_align.v"),
        Path("Hardware/cpu/mem/cpu_stack.v"),
        Path("Hardware/cpu/mem/cpu_io_regs.v"),
        Path("Hardware/cpu/mem/cpu_interrupts.v"),
        Path("Hardware/cpu/wb/cpu_wb.v"),
    ]
    tests_dir: Path = Path("Tests/cpu")
    modules_dir: Path = Path("Tests/cpu/modules")
    gtkwave_rc: Path = Path("Tests/cpu/modules/gtkwaverc")
    build_dir: Path = Path("build/cpu")
    wave_dir: Path = Path("build/cpu/wave")
    tool_dirs: list[Path] = [Path("Tools/b32asm"), Path("Tools/b32sim")]
    harness_dir: Path = Path("Hardware/cpu/harness")
    harness_top: str = "b32p4_harness"
    timing_dir: Path = Path("build/cpu/timing")
    quartus_bin: Path = Path("/home/bart/altera_lite/25.1std/quartus/bin")
    device: str = "EP4CE40F23I7"
    clock_period_ns: float = 10.0


class Config(BaseModel):
    build_dir: Path = Path("build")
    cpu: CpuConfig = CpuConfig()
    website: WebsiteConfig = WebsiteConfig()


CONFIG = Config()

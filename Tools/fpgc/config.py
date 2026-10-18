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
        Path("Hardware/cpu/ex/cpu_alu.v"),
    ]
    tests_dir: Path = Path("Tests/cpu")
    modules_dir: Path = Path("Tests/cpu/modules")
    gtkwave_rc: Path = Path("Tests/cpu/modules/gtkwaverc")
    build_dir: Path = Path("build/cpu")
    wave_dir: Path = Path("build/cpu/wave")
    tool_dirs: list[Path] = [Path("Tools/b32asm"), Path("Tools/b32sim")]
    harness_dir: Path = Path("Hardware/cpu/harness")
    timing_dir: Path = Path("build/cpu/timing")
    quartus_bin: Path = Path("/home/bart/altera_lite/25.1std/quartus/bin")
    device: str = "EP4CE40F23I7"
    clock_period_ns: float = 10.0


class Config(BaseModel):
    build_dir: Path = Path("build")
    cpu: CpuConfig = CpuConfig()
    website: WebsiteConfig = WebsiteConfig()


CONFIG = Config()

"""All settings of the fpgc commands. Paths are relative to the repository root."""

from pathlib import Path

from pydantic import BaseModel


class WebsiteConfig(BaseModel):
    source_dir: Path = Path("Website")
    theme_dir: Path = Path("Website/themes/hextra")
    site_dir: Path = Path("build/website")
    port: int = 8088
    deploy_target: str = "192.168.0.240:/home/bart/NFS/fpgc/"


class Config(BaseModel):
    build_dir: Path = Path("build")
    website: WebsiteConfig = WebsiteConfig()


CONFIG = Config()

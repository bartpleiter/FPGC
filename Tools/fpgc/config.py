"""All settings of the fpgc commands. Paths are relative to the repository root."""

from pathlib import Path

from pydantic import BaseModel


class DocsConfig(BaseModel):
    mkdocs_config: Path = Path("Docs/mkdocs.yml")
    site_dir: Path = Path("build/docs")
    port: int = 8088
    deploy_target: str = "192.168.0.240:/home/bart/NFS/fpgc/"


class Config(BaseModel):
    build_dir: Path = Path("build")
    docs: DocsConfig = DocsConfig()


CONFIG = Config()

"""Documentation website."""

import click

from fpgc.common import run
from fpgc.config import CONFIG

cfg = CONFIG.docs


@click.group()
def docs() -> None:
    """Documentation website."""


@docs.command()
@click.option("--port", default=cfg.port, show_default=True)
def serve(port: int) -> None:
    """Serve the docs locally with live reload."""
    run("mkdocs", "serve", "-f", cfg.mkdocs_config, "-a", f"localhost:{port}")


@docs.command()
def build() -> None:
    """Build the docs into build/docs."""
    # MkDocs resolves a relative -d against Docs/, so it gets an absolute path
    run(
        "mkdocs",
        "build",
        "--strict",
        "-f",
        cfg.mkdocs_config,
        "-d",
        cfg.site_dir.absolute(),
    )


@docs.command()
@click.pass_context
def deploy(ctx: click.Context) -> None:
    """Build the docs and publish."""
    ctx.invoke(build)
    run("rsync", "-r", "--delete", f"{cfg.site_dir}/", cfg.deploy_target)

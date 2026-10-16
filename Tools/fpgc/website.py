"""Website with the documentation and the blog."""

import click

from fpgc.common import run
from fpgc.config import CONFIG

cfg = CONFIG.website


@click.group()
def website() -> None:
    """Website with the documentation and the blog."""
    if not (cfg.theme_dir / "theme.toml").exists():
        raise click.UsageError(
            "the Hextra theme is missing, run: git submodule update --init"
        )


@website.command()
@click.option("--port", default=cfg.port, show_default=True)
def serve(port: int) -> None:
    """Serve the website locally with live reload, including drafts."""
    run(
        "hugo",
        "server",
        "--source",
        cfg.source_dir,
        "--port",
        str(port),
        "--buildDrafts",
        "--renderToMemory",
    )


@website.command()
def build() -> None:
    """Build the website into build/website."""
    # Hugo resolves a relative --destination against --source, so it gets an absolute path
    run(
        "hugo",
        "--source",
        cfg.source_dir,
        "--destination",
        cfg.site_dir.absolute(),
        "--minify",
        "--cleanDestinationDir",
        "--panicOnWarning",
    )


@website.command()
@click.pass_context
def deploy(ctx: click.Context) -> None:
    """Build the website and publish."""
    ctx.invoke(build)
    run("rsync", "-r", "--delete", f"{cfg.site_dir}/", cfg.deploy_target)

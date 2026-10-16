# Website

How the FPGC website works and how to add documentation and blog posts.

The website at https://fpgc.bartpleiter.com holds the documentation and the blog. It's a static site built with [Hugo](https://gohugo.io/) and the [Hextra](https://imfing.github.io/hextra/) theme. The source is in `Website/`, and everything is written in Markdown. See `build-system.md` for the general rules.

## Commands

```bash
fpgc website serve     # live preview at http://localhost:8088, including drafts
fpgc website build     # build into build/website; fails on any Hugo warning
fpgc website deploy    # build, then deploy to server
```

## Layout

```
Website/
  hugo.yaml              site config: menu, theme settings, links
  content/
    _index.md            documentation home page (the site root)
    <section>/           documentation: one folder per section
    blog/                blog posts: one folder per post
  static/images/         site-wide images (logo)
  assets/css/custom.css  colour overrides
  themes/hextra/         the theme: a git submodule, never edited
```

The documentation is the site root: `content/_index.md` sets `type: docs` for every page under it, so they all get the docs layout with a sidebar. `content/blog/_index.md` switches its pages back to the blog layout and keeps them out of the docs sidebar.

## Adding a documentation page

Create `Website/content/<page>.md`, or `Website/content/<section>/<page>.md`:

```markdown
---
title: Memory map
weight: 3
---

Text...
```

- **Sections:** a section is a folder with an `_index.md` (title, weight and intro text).
- **Order:** the sidebar is ordered by `weight`, lowest first. Pages without a weight come after, alphabetically.
- **Images:** for images that belong to one page, make the page a folder (`memory-map/index.md`) and put the images next to `index.md`.

## Adding a blog post

Create `Website/content/blog/<slug>/index.md`, with the images in the same folder:

```markdown
---
title: Designing the CPU pipeline
date: 2026-11-01
authors:
  - name: Bart Pleiter
    link: https://github.com/bartpleiter
tags: [cpu]
---

One or two sentences, shown in the blog list.

<!--more-->

The rest of the post. ![Pipeline diagram](pipeline.svg)
```

The slug becomes the URL: `/blog/<slug>/`. Posts are sorted by `date`, newest first.

## Writing conventions

- **Plain Markdown first.** For notes and warnings, use GitHub-style alerts (`> [!NOTE]`, `> [!WARNING]`), which also render on GitHub. Use Hextra shortcodes (tabs, steps, cards) only when they really help, because they tie content to this theme.
- **Code blocks name their language:** `verilog`, `c`, `python`, `bash`. For B32P assembly, use `asm` until there's a dedicated highlighter.
- **Links to other pages** are relative `.md` links (`[CPU](../hardware/cpu.md)`). They work on GitHub and on the site, but the build doesn't check them. Use `[CPU]({{< ref "hardware/cpu" >}})` when a broken link should fail the build.
- **Images:** prefer SVG for diagrams, and compress PNGs before committing. They stay in the git history forever.
- **No drafts in git.** Hugo has `draft: true`, but this repository is public. Keep unfinished posts local until they're ready.

## Theme and Hugo versions

- **Hugo** is pinned to an exact version in `pyproject.toml` (the `hugo` PyPI package ships the official binary). `uv sync` installs it into the venv.
- **Hextra** is a git submodule in `Website/themes/hextra`, pinned to a release tag. `git submodule status` shows the version. After a fresh clone, run `git submodule update --init` (or clone with `--recurse-submodules`); `fpgc website` tells you when the theme is missing.
- **Never edit files in `themes/hextra`.** To change a template, copy it to the same path under `Website/layouts/` and edit the copy. Colours and CSS go in `Website/assets/css/custom.css`.
- **Upgrade Hugo and Hextra together**, because a new Hugo version can break an older theme:
  1. `git -C Website/themes/hextra fetch --tags` and `git -C Website/themes/hextra checkout <new tag>`.
  2. Update the `hugo==` pin in `pyproject.toml` and run `uv sync`.
  3. Run `fpgc website build` (it fails on deprecation warnings), and check the result with `fpgc website serve`.
  4. Commit the submodule change together with `pyproject.toml` and `uv.lock`.
- **Optional theme features** (Mermaid, KaTeX, image zoom) download their JavaScript from a CDN at build time and serve it from our own domain. Some default to `@latest`, so when you turn one on, pin its version in `hugo.yaml` (see the Hextra configuration docs).

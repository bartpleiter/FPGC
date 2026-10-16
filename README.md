# FPGC

![FPGC Logo](Website/static/images/logo.png)

## Current state

Currently doing a large refactor without changing main functionality, trying to simplify and clean up the codebase, going over all modules again. For this, the previous repo [FPGC-reference](https://github.com/bartpleiter/FPGC-reference) is used for reference and validation.

## Quick start

All project tasks should be run using the `fpgc` tool from the repository root:

```bash
# Setup
git submodule update --init  # fetch all subrepositories if you did not clone with --recurse-submodules
uv sync                      # install dependencies and the fpgc command
source .venv/bin/activate    # activate the virtual environment

# Usage
fpgc -h                      # list all commands
fpgc website serve           # serve docs and blog website locally
```

# Python environment

This project uses Python for the following components:

- Assembler (except the assembler that runs on the FPGC itself, as that one is written in C)
- Scripts (e.g. uploading software to the FPGC via a web socket)
- MkDocs documentation site

This project uses `uv` to manage the python environment.
To setup the environment for this project, you just need to install `uv` and run from the project root: `uv sync`.

!!! note
    All scripts in this project are expected to be run from the project root.

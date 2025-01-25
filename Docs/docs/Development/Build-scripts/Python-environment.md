# Python environment

This project uses Python for the following components:

- Assembler (except the assembler that runs on the FPGC itself, as that one is written in C)
- Scripts (e.g. uploading software to the FPGC via a web socket)
- MkDocs documentation site

For this project it is expected to have a conda environment named FPGC running `Python 3.12` with some required packages.
Follow these instructions after installing (mini/ana)conda:

``` bash
conda create --name FPGC python=3.12
conda activate FPGC
pip install -r requirements.txt
conda deactivate # Since the bash scripts will activate when needed
```

Note that all scripts in this project are expected to be run from the project root.

!!! note
    This should be updated to use poetry venv instead

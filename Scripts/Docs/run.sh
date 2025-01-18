#!/bin/bash

# Activate conda environment
eval "$(conda shell.bash hook)"
conda activate FPGC

cd Docs

python3 -m mkdocs serve -a localhost:8088

conda deactivate

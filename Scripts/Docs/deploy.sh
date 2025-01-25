#!/bin/bash

# Activate conda environment
eval "$(conda shell.bash hook)"
conda activate FPGC

cd Docs

python3 -m mkdocs build --clean

# Obviously this will only work on my personal server
ssh -p 2222 b4rt.nl 'rm -rf /home/bart/PV/fpgc/'
rsync -r -e 'ssh -p 2222' site/ b4rt.nl:/home/bart/PV/fpgc/
ssh -p 2222 b4rt.nl 'chown $USER:$USER -R /home/bart/PV/fpgc/'

conda deactivate

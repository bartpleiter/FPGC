#!/bin/bash

# Activate conda environment
eval "$(conda shell.bash hook)"
conda activate FPGC

cd Docs

python3 -m mkdocs build --clean

# Obviously this will only work on my personal server
ssh b4rt.nl 'rm -rf /var/www/b4rt.nl/html/fpgc/'
rsync -r site/ b4rt.nl:/var/www/b4rt.nl/html/fpgc/
ssh b4rt.nl 'chown $USER:www-data -R /var/www/b4rt.nl/html/fpgc/'

conda deactivate

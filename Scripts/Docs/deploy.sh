#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

cd Docs

python3 -m mkdocs build --clean

# Obviously this will only work on my personal server
ssh 192.168.0.240 'rm -rf /home/bart/NFS/fpgc/*'
rsync -r site/ 192.168.0.240:/home/bart/NFS/fpgc/
ssh 192.168.0.240 'chown $USER:$USER -R /home/bart/NFS/fpgc/*'

# Deactivate virtual environment
deactivate

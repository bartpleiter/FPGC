#!/bin/bash

# Activate the virtual environment
source .venv/bin/activate

cd Docs

python3 -m mkdocs serve -a localhost:8088

# Deactivate virtual environment
deactivate

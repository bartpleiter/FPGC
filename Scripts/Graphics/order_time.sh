#!/bin/bash

start="$(date +%Y-%m-%d) 09:00:00"

find . -maxdepth 1 -name 'img_*.png' \
| sort -V \
| while read -r file; do
    touch -d "$start" "$file"
    start=$(date -d "$start + 1 minute" '+%Y-%m-%d %H:%M:%S')
done

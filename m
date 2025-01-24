#!/bin/sh

set -e

if [ $# -eq 0 ]
then
    echo "usage: m d|r|p|a [target]"
    exit 1
fi
python3 src/configure.py
third_party/ninja/ninja-mac -C out/m$1 $2 $3 $4 $5

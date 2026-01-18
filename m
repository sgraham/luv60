#!/bin/sh

set -e

if [ $# -eq 0 ]
then
    echo "usage: m d|r|p|a [target]"
    exit 1
fi
python3 src/configure.py

OS="$(uname -s)"
ARCH="$(uname -m)"
THIS_DIR="$(dirname "${0}")"

case "$OS" in
  Darwin)    exec "${THIS_DIR}/third_party/ninja/ninja-mac-$ARCH" -C out/m$1 $2 $3 $4 $5;;
  Linux)     exec "${THIS_DIR}/third_party/ninja/ninja-linux-$ARCH" -C out/la$1 $2 $3 $4 $5;;
  *)         echo "Unsupported OS ${OS}"
             exit 1;;
esac

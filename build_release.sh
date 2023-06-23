#!/usr/bin/env bash

set -x

DATE=$(date '+%y%m%d%H%M%S')
SRC=$(pwd)

INSTALL_DIR=${SRC}/${DATE}_release.install
BUILD_DIR=${SRC}/${DATE}_release.build

echo "install: $INSTALL_DIR, build: $BUILD_DIR"

mkdir -p "$INSTALL_DIR"
cmake -D CMAKE_BUILD_TYPE=Release -S . -B "$BUILD_DIR"

ln -fs -T "$INSTALL_DIR" install
ln -fs -T "$BUILD_DIR" build

cd "$BUILD_DIR" || exit

cmake --build . # --verbose
cmake --install . --prefix "$INSTALL_DIR" # --verbose

#!/usr/bin/env bash

set -x

DATE=$(date '+%y%m%d%H%M%S')
SRC=$(pwd)

INSTALL_DIR=$SRC/$DATE.install
BUILD_DIR=$SRC/$DATE.build

mkdir -p "$INSTALL_DIR"

echo "install: $INSTALL_DIR, build: $BUILD_DIR"

cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_FLAGS="-fdiagnostics-color=always" -D CMAKE_VERBOSE_MAKEFILE=1 -S . -B "$BUILD_DIR"

ln -fs -T "$INSTALL_DIR" install
ln -fs -T "$BUILD_DIR" build

cd "$BUILD_DIR" || exit

cmake --build . --verbose
cmake --install . --prefix "$INSTALL_DIR" --verbose

# cp -f "$INSTALL_DIR"/bin/* "$SRC"/xchg/bin

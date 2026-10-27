#!/bin/bash

git submodule update --init --recursive

# check if ninja is available, if so use it, otherwise fallback to default cmake build
if command -v ninja &> /dev/null; then
    echo "Ninja found, using it for build"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
else
    echo "Ninja not found, using default cmake build"
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

cmake --build build --target TaskManager --parallel
cmake --install build --prefix ~/.local

#!/bin/bash

# output directory

target="Build/$1"

# generate cmake build files

emcmake cmake -S . -B $target -DCMAKE_BUILD_TYPE=$1 -G "Unix Makefiles" -DEMSCRIPTEN="true"

# compile cmake build files

cmake --build $target --config $1

cp "$target/compile_commands.json" . 

#!/bin/bash

target="Build"

if [[ -d "$target" ]]; then
    if [[ -d "$target/Debug/bin" ]]; then
        target="$target/Debug/bin"
    elif [[ -d "$target/Release/bin" ]]; then
        target="$target/Release/bin"
    else
        echo "Can't find folder Debug or Release inside $target"
        exit 1
    fi
else
    echo "Run build.sh first"
    exit 1
fi

# Make executable if not already
# chmod +x "$target/EbonyEditor"

# Launch the editor
cd "$target"
./Kasino

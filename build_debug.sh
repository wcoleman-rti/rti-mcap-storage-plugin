#!/usr/bin/bash

# Detect script directory
script="${BASH_SOURCE[0]}"
script_dir=`dirname "$script"`
script_dir=`realpath "$script_dir"`

cd "$script_dir"
mkdir -p build-debug

# Debug
cd "${script_dir}/build-debug"
cmake -DCMAKE_BUILD_TYPE=DEBUG ..
cmake --build .

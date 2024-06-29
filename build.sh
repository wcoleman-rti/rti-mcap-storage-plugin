#!/usr/bin/bash

# Detect script directory
script="${BASH_SOURCE[0]}"
script_dir=`dirname "$script"`
script_dir=`realpath "$script_dir"`

cd "$script_dir"
mkdir -p build

# Release
cd "${script_dir}/build"
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=RELEASE ..
cmake --build .

# Debug
cd "${script_dir}/build"
cmake -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=DEBUG ..
cmake --build .

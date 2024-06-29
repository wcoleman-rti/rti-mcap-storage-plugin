#!/usr/bin/bash

# Detect script directory
script="${BASH_SOURCE[0]}"
script_dir=`dirname "$script"`
script_dir=`realpath "$script_dir"`

# update submodules
git submodule update --init --recursive


# checkout MCAP tag releases/cpp/v1.4.0
# https://github.com/foxglove/mcap/tree/releases/cpp/v1.4.0
git -C "${script_dir}/external/mcap" checkout releases/cpp/v1.4.0
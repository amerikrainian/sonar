#!/bin/sh

CONFIG=${1:-release}
CONFIG_UPPER=$(printf '%s' "$CONFIG" | tr '[:lower:]' '[:upper:]')

cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=$CONFIG_UPPER \
  --log-level=ERROR -Wno-dev

cmake --build build -j

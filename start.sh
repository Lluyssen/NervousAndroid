#!/usr/bin/env bash
set -e

cmake -S . -B build
cmake --build build -j"$(nproc)"

#./build/test_rules || exit 1
#./build/test_protocol || exit 1
./build/nervous_server 5000 20 20
./build/nervous_game

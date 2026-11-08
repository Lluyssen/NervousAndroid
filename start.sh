#!/usr/bin/env bash
set -e

cmake -S . -B build
cmake --build build -j"$(nproc)"

./build/test_rules || exit 1
./build/nervous_game

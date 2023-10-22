#!/usr/bin/env bash
#
# Uses build directory ./b/
#
cmake -Hall -Bb
cmake --build b
./build/standalone/glob --help

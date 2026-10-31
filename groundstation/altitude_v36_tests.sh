#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_altitude36_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/firmware/src" \
  "$projectRoot/groundstation/test_altitude36.cpp" -o "$outDir/altitude"
"$outDir/altitude"
bash "$projectRoot/groundstation/routing_v35_tests.sh"

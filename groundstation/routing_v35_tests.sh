#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_routing35_host"
mkdir -p "$outDir"
python3 "$projectRoot/groundstation/test_motor_routing.py"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
  "$projectRoot/groundstation/mission_host/test_routing35.cpp" -o "$outDir/routing35"
"$outDir/routing35"
bash "$projectRoot/groundstation/approach_v34_tests.sh"

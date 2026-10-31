#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_reacquire_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
  "$projectRoot/groundstation/mission_host/test_reacquire_epoch.cpp" -o "$outDir/epoch"
"$outDir/epoch"
bash "$projectRoot/groundstation/near_v31_tests.sh"
bash "$projectRoot/groundstation/vertical_v30_tests.sh"

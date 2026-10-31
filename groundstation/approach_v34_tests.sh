#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_approach34_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
  "$projectRoot/groundstation/mission_host/test_approach34.cpp" -o "$outDir/approach"
"$outDir/approach"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/firmware/src" \
  "$projectRoot/groundstation/test_support_yaw.cpp" -o "$outDir/mixer"
"$outDir/mixer"
bash "$projectRoot/groundstation/lift_v33_tests.sh"

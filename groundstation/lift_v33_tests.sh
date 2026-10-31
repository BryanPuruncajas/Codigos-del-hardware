#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_lift33_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
  "$projectRoot/groundstation/mission_host/test_lift33.cpp" -o "$outDir/lift33"
"$outDir/lift33"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/firmware/src" \
  "$projectRoot/groundstation/test_visual_search.cpp" -o "$outDir/search"
"$outDir/search"
bash "$projectRoot/groundstation/reacquire_v32_tests.sh"

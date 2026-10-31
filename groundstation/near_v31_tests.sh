#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_near_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/firmware/src" \
  "$projectRoot/groundstation/test_near_support.cpp" -o "$outDir/near_support"
"$outDir/near_support"
for name in test_near_approach test_single_waypoint test_mission test_partial_start test_yaw_forecast; do
  g++ -std=c++11 -Wall -Wextra -Werror \
    -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
    -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/mission_host/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done

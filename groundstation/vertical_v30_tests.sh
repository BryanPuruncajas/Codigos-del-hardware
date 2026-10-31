#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_v30_host"
mkdir -p "$outDir"
for name in test_vertical_v30 test_vertical_confidence test_visual_support test_vertical_pipeline test_inertial_height test_visual_height_priority; do
  g++ -std=c++11 -Wall -Wextra -Werror \
    -I"$projectRoot/firmware/src" -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done
for name in test_mission test_partial_start test_yaw_forecast test_single_waypoint test_p03_visual; do
  g++ -std=c++11 -Wall -Wextra -Werror \
    -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
    -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/mission_host/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done

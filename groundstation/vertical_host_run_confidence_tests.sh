#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_confidence_host"
mkdir -p "$outDir"
for name in test_vertical_confidence test_inertial_height test_visual_height_priority test_vertical_pipeline; do
  g++ -std=c++11 -Wall -Wextra -Werror \
    -I"$projectRoot/firmware/src" -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done
for name in test_mission test_partial_start test_yaw_forecast; do
  g++ -std=c++11 -Wall -Wextra -Werror \
    -I"$projectRoot/groundstation/mission_host" -I"$projectRoot/firmware/src" \
    -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/mission_host/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/firmware/src" "$projectRoot/groundstation/replay_vertical_confidence.cpp" \
  -o "$outDir/replay_vertical_confidence"
echo "Replay executable: $outDir/replay_vertical_confidence"

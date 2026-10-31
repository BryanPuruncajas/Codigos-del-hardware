#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_flow38_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/firmware/src" -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/test_flow_height38.cpp" -o "$outDir/height"
"$outDir/height"
g++ -std=c++11 -Wall -Wextra -Werror -I"$projectRoot/groundstation/mission_host" \
    -I"$projectRoot/firmware/src" -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
    "$projectRoot/groundstation/mission_host/test_flow_mission38.cpp" -o "$outDir/mission"
"$outDir/mission"
bash "$projectRoot/groundstation/fixed_height_v37_tests.sh"
bash "$projectRoot/flowdeck_sender/test_flow_sender.sh"

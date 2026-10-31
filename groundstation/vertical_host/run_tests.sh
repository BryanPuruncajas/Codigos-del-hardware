#!/usr/bin/env bash
set -eu
projectRoot=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_vertical_host"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/groundstation/vertical_host" \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" \
  -I"$projectRoot/firmware/.pio/libdeps/seeed_xiao_esp32s3/Adafruit BMP3XX Library" \
  "$projectRoot/groundstation/vertical_host/test_bmp390_pipeline.cpp" \
  "$projectRoot/firmware/lib/BlimpSwarm/src/sense/Barometer390.cpp" \
  -o "$outDir/bmp390"
"$outDir/bmp390"
g++ -std=c++11 -Wall -Wextra -Werror \
  -I"$projectRoot/firmware/lib/BlimpSwarm/src" -I"$projectRoot/firmware/src" \
  "$projectRoot/groundstation/test_vertical_pipeline.cpp" -o "$outDir/pipeline"
"$outDir/pipeline"
g++ -std=c++11 -Wall -Wextra -Werror \
  "$projectRoot/groundstation/test_vertical_estimate.cpp" -o "$outDir/estimate"
"$outDir/estimate"

#!/usr/bin/env bash
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
outDir="${TMPDIR:-/tmp}/blimp_flowonly39"
mkdir -p "$outDir"
g++ -std=c++11 -Wall -Wextra -Werror -I"$root/firmware/src" -I"$root/firmware/lib/BlimpSwarm/src" \
    "$root/groundstation/test_flow_only39.cpp" -o "$outDir/flow"
"$outDir/flow"
g++ -std=c++11 -Wall -Wextra -Werror -I"$root/firmware/src" -I"$root/firmware/lib/BlimpSwarm/src" \
    "$root/groundstation/test_flow_stability41.cpp" -o "$outDir/stability41"
"$outDir/stability41"
g++ -std=c++11 -Wall -Wextra -Werror -I"$root/groundstation/mission_host" -I"$root/firmware/src" \
    -I"$root/firmware/lib/BlimpSwarm/src" "$root/groundstation/mission_host/test_flow_mission39.cpp" -o "$outDir/mission"
"$outDir/mission"
python3 "$root/groundstation/test_no_barometer39.py"
python3 "$root/groundstation/test_flow_safety39.py"
python3 "$root/groundstation/test_motor_routing.py"
for name in test_support_yaw test_inertial_height; do
  g++ -std=c++11 -Wall -Wextra -Werror -I"$root/firmware/src" -I"$root/firmware/lib/BlimpSwarm/src" \
    "$root/groundstation/$name.cpp" -o "$outDir/$name"
  "$outDir/$name"
done
bash "$root/flowdeck_sender/test_flow_sender.sh"

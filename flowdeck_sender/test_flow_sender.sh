#!/usr/bin/env bash
set -eu
senderRoot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmp "$senderRoot/FlowdeckEspNow/FlowProtocol.h" "$senderRoot/../firmware/lib/BlimpSwarm/src/comm/FlowProtocol.h"
outDir=$(mktemp -d)
trap 'rm -f "$outDir/flow_parser"; rmdir "$outDir"' EXIT
g++ -std=c++11 -Wall -Wextra -Werror -I"$senderRoot/FlowdeckEspNow" \
    "$senderRoot/test_flow_parser.cpp" -o "$outDir/flow_parser"
"$outDir/flow_parser"

#!/bin/sh
set -u

here=$(cd "$(dirname "$0")/.." && pwd)
stamp=${STAMP:-$(date +%Y%m%d)}
root=${OUT_ROOT:-$here/results/matched-$stamp}
maps=${MAPS:-$root/maps}

: "${DRAGOMAN:?set DRAGOMAN to the dragoman command}"
: "${OD_SERVER:?}" "${OD_DATA:?}" "${OD_GAME:?}" "${GD5_PYTHON:?}" "${GD5_DIR:?}" "${GD5_SOURCE_DIR:?set GD5_SOURCE_DIR to a GD5 checkout to convert into}"

mkdir -p "$maps"
python3 "$here/tools/matched_maps.py" --dragoman "$DRAGOMAN" \
    --gd5-scenario "$GD5_SOURCE_DIR/scenarios/historical/1939" --odmap "$maps/gd5-1939.odmap" \
    --od-source "$OD_DATA/STDmaps/1939.odmap" --gd5-map "$GD5_SOURCE_DIR/base_maps/OJH_OD_1939" > "$maps/conversion.json"

export OD_SERVER OD_DATA OD_GAME GD5_PYTHON GD5_DIR
export METRICS="${METRICS:-tpm net fps}"

OUT="$root/gd5-1939" OD_MAP="$maps/gd5-1939.odmap" OD_SAVE="$maps/gd5-1939.odmap" \
    GD5_SCENARIO="scenarios/historical/1939" PLAYERS=35 FREECIV_SIZE=3 UNCIV_SIZE="${UNCIV_SIZE_A:-medium}" \
    REPEATS="${REPEATS_A:-3}" sh "$here/tools/run-all.sh"

OUT="$root/od-1939" OD_MAP="$OD_DATA/STDmaps/1939.odmap" OD_SAVE="$OD_DATA/STDmaps/1939.odmap" \
    GD5_SCENARIO="$GD5_SOURCE_DIR/base_maps/OJH_OD_1939" PLAYERS=63 FREECIV_SIZE=4 UNCIV_SIZE="${UNCIV_SIZE_B:-large}" \
    REPEATS="${REPEATS_B:-1}" sh "$here/tools/run-all.sh"

echo "matched results: $root/gd5-1939 and $root/od-1939"

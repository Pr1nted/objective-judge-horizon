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

extra_games() {
    tier_out=$1 players=$2 stars=$3 map=$4 repeats=$5
    work="$tier_out/work-extra"
    if [ -n "${FREEORION_SERVER:-}" ]; then
        echo "== FreeOrion turn speed"
        FREEORION_STARS=$stars "$here/build/ojh" tpm "$here/drivers/freeorion/freeorion.json" --turns "${TURNS:-250}" \
            --repeat "$repeats" --players "$players" --work "$work" --out "$tier_out/freeorion-tpm.json" >> "$tier_out/run.log" 2>&1 \
            && echo "   done" || echo "   did not finish; the result file records why"
    fi
    if [ -n "${FREECOL_DIR:-}" ]; then
        echo "== FreeCol turn speed"
        FREECOL_EUROPEANS=8 FREECOL_MAP=$map "$here/build/ojh" tpm "$here/drivers/freecol/freecol.json" --turns "${TURNS:-250}" \
            --repeat "$repeats" --work "$work" --out "$tier_out/freecol-tpm.json" >> "$tier_out/run.log" 2>&1 \
            && echo "   done" || echo "   did not finish; the result file records why"
    fi
    rm -rf "$work"
    rm -rf "$tier_out/graphs" "$tier_out"/score-* "$tier_out/report.md" "$tier_out/report.txt"
    "$here/build/ojh" report "$tier_out" > /dev/null
}
export METRICS="${METRICS:-tpm net fps}"

OUT="$root/gd5-1939" OD_MAP="$maps/gd5-1939.odmap" OD_SAVE="$maps/gd5-1939.odmap" \
    GD5_SCENARIO="scenarios/historical/1939" PLAYERS=35 FREECIV_SIZE=3 UNCIV_SIZE="${UNCIV_SIZE_A:-medium}" \
    REPEATS="${REPEATS_A:-3}" sh "$here/tools/run-all.sh"
extra_games "$root/gd5-1939" 35 906 40x100 "${REPEATS_A:-3}"

OUT="$root/od-1939" OD_MAP="$OD_DATA/STDmaps/1939.odmap" OD_SAVE="$OD_DATA/STDmaps/1939.odmap" \
    GD5_SCENARIO="$GD5_SOURCE_DIR/base_maps/OJH_OD_1939" PLAYERS=63 FREECIV_SIZE=4 UNCIV_SIZE="${UNCIV_SIZE_B:-large}" \
    REPEATS="${REPEATS_B:-1}" sh "$here/tools/run-all.sh"
extra_games "$root/od-1939" 63 1298 48x120 "${REPEATS_B:-1}"

echo "matched results: $root/gd5-1939 and $root/od-1939"

#!/bin/sh
set -u

here=$(cd "$(dirname "$0")/.." && pwd)
ojh=${OJH:-$here/build/ojh}
out=${OUT:-$here/results/run-$(date +%Y%m%d-%H%M)}
turns=${TURNS:-250}
repeats=${REPEATS:-3}
net_turns=${NET_TURNS:-20}
clients=${CLIENTS:-2}
fps_seconds=${FPS_SECONDS:-5}
fps_timeout=${FPS_TIMEOUT:-900}
seed=${SEED:-20260914}
java_bin=${JAVA_BIN:-}

od_server=${OD_SERVER:-}
od_data=${OD_DATA:-}
od_game=${OD_GAME:-}
od_save=${OD_SAVE:-}
gd5_python=${GD5_PYTHON:-}
gd5_dir=${GD5_DIR:-}
unciv_jar=${UNCIV_JAR:-}
freeciv_server=${FREECIV_SERVER:-freeciv-server}
freeciv_client=${FREECIV_CLIENT:-freeciv-gtk4}

mkdir -p "$out"
work="$out/work"
log="$out/run.log"
: > "$log"

java_options=""
if [ -n "$java_bin" ]; then
    java_options="--java $java_bin/java --javac $java_bin/javac --jar $java_bin/jar"
fi

measure() {
    name=$1
    shift
    echo "== $name" | tee -a "$log"
    if "$ojh" "$@" >>"$log" 2>&1; then
        echo "   done" | tee -a "$log"
    else
        echo "   did not finish; the result file records why" | tee -a "$log"
    fi
}

common="--seed $seed --drivers $here/drivers --work $work"

if [ -n "$od_server" ] && [ -n "$od_data" ]; then
    measure "Open Doctrines turn speed" tpm opendoctrines --turns "$turns" --repeat "$repeats" $common --od-server "$od_server" --od-data "$od_data" --out "$out/opendoctrines-tpm.json"
    if [ -n "$od_save" ]; then
        measure "Open Doctrines footprint" footprint opendoctrines $common --od-server "$od_server" --od-data "$od_data" --od-save "$od_save" --out "$out/opendoctrines-footprint.json"
    else
        measure "Open Doctrines footprint" footprint opendoctrines $common --od-server "$od_server" --od-data "$od_data" --out "$out/opendoctrines-footprint.json"
    fi
    measure "Open Doctrines network" net opendoctrines --turns "$net_turns" --clients "$clients" $common --od-server "$od_server" --od-data "$od_data" --out "$out/opendoctrines-net.json"
    if [ -n "$od_game" ]; then
        if [ -n "$od_save" ]; then
            measure "Open Doctrines frame rate" fps opendoctrines --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --od-game "$od_game" --od-data "$od_data" --od-save "$od_save" --out "$out/opendoctrines-fps.json"
        else
            measure "Open Doctrines frame rate" fps opendoctrines --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --od-game "$od_game" --od-data "$od_data" --out "$out/opendoctrines-fps.json"
        fi
    fi
fi

if [ -n "$gd5_python" ] && [ -n "$gd5_dir" ]; then
    measure "Greater Diplomacy 5 turn speed" tpm gd5 --turns "$turns" --repeat "$repeats" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" --out "$out/gd5-tpm.json"
    measure "Greater Diplomacy 5 footprint" footprint gd5 --turns 20 $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" --out "$out/gd5-footprint.json"
    measure "Greater Diplomacy 5 network" net gd5 --turns "$net_turns" --clients "$clients" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" --out "$out/gd5-net.json"
    measure "Greater Diplomacy 5 frame rate" fps gd5 --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" --out "$out/gd5-fps.json"
fi

if command -v "$freeciv_server" >/dev/null 2>&1; then
    measure "Freeciv turn speed" tpm freeciv --turns "$turns" --repeat "$repeats" $common --freeciv-server "$freeciv_server" --out "$out/freeciv-tpm.json"
    measure "Freeciv footprint" footprint freeciv --turns 20 $common --freeciv-server "$freeciv_server" --out "$out/freeciv-footprint.json"
    measure "Freeciv network" net freeciv --turns "$net_turns" --clients "$clients" $common --freeciv-server "$freeciv_server" --freeciv-client "$freeciv_client" --out "$out/freeciv-net.json"
    measure "Freeciv frame rate" fps freeciv $common --out "$out/freeciv-fps.json"
fi

if [ -n "$unciv_jar" ]; then
    measure "Unciv turn speed" tpm unciv --turns "$turns" --repeat "$repeats" $common --unciv-jar "$unciv_jar" $java_options --out "$out/unciv-tpm.json"
    measure "Unciv footprint" footprint unciv --turns 20 $common --unciv-jar "$unciv_jar" $java_options --out "$out/unciv-footprint.json"
    measure "Unciv network" net unciv --turns "$net_turns" $common --unciv-jar "$unciv_jar" $java_options --out "$out/unciv-net.json"
    measure "Unciv frame rate" fps unciv --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --unciv-jar "$unciv_jar" $java_options --out "$out/unciv-fps.json"
fi

rm -rf "$work"
"$ojh" report "$out" | tee -a "$log"
echo "results, report and graphs: $out"

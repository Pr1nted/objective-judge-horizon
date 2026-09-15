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
tpm_timeout=${TPM_TIMEOUT:-3600}
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
players=${PLAYERS:-}
od_map=${OD_MAP:-}
gd5_scenario=${GD5_SCENARIO:-}
freeciv_size=${FREECIV_SIZE:-}
unciv_size=${UNCIV_SIZE:-}
od_world="${od_map:+--od-map $od_map}"
gd5_world="${gd5_scenario:+--gd5-scenario $gd5_scenario}"
freeciv_world="${players:+--players $players} ${freeciv_size:+--map-size $freeciv_size}"
unciv_world="${players:+--players $players} ${unciv_size:+--map-size $unciv_size}"

metrics=${METRICS:-tpm footprint net fps}
wants() {
    case " $metrics " in
        *" $1 "*) return 0 ;;
    esac
    return 1
}

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
    wants tpm && measure "Open Doctrines turn speed" tpm opendoctrines --turns "$turns" --repeat "$repeats" --timeout "$tpm_timeout" $common --od-server "$od_server" --od-data "$od_data" $od_world --out "$out/opendoctrines-tpm.json"
    if [ -n "$od_save" ]; then
        wants footprint && measure "Open Doctrines footprint" footprint opendoctrines $common --od-server "$od_server" --od-data "$od_data" --od-save "$od_save" --out "$out/opendoctrines-footprint.json"
    else
        wants footprint && measure "Open Doctrines footprint" footprint opendoctrines $common --od-server "$od_server" --od-data "$od_data" --out "$out/opendoctrines-footprint.json"
    fi
    wants net && measure "Open Doctrines network" net opendoctrines --turns "$net_turns" --clients "$clients" $common --od-server "$od_server" --od-data "$od_data" $od_world --out "$out/opendoctrines-net.json"
    if [ -n "$od_game" ]; then
        if [ -n "$od_save" ]; then
            wants fps && measure "Open Doctrines frame rate" fps opendoctrines --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --od-game "$od_game" --od-data "$od_data" --od-save "$od_save" --out "$out/opendoctrines-fps.json"
        else
            wants fps && measure "Open Doctrines frame rate" fps opendoctrines --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --od-game "$od_game" --od-data "$od_data" --out "$out/opendoctrines-fps.json"
        fi
    fi
fi

if [ -n "$gd5_python" ] && [ -n "$gd5_dir" ]; then
    wants tpm && measure "Greater Diplomacy 5 turn speed" tpm gd5 --turns "$turns" --repeat "$repeats" --timeout "$tpm_timeout" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" $gd5_world --out "$out/gd5-tpm.json"
    wants footprint && measure "Greater Diplomacy 5 footprint" footprint gd5 --turns 20 $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" $gd5_world --out "$out/gd5-footprint.json"
    wants net && measure "Greater Diplomacy 5 network" net gd5 --turns "$net_turns" --clients "$clients" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" $gd5_world --out "$out/gd5-net.json"
    wants fps && measure "Greater Diplomacy 5 frame rate" fps gd5 --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --gd5-python "$gd5_python" --gd5-dir "$gd5_dir" $gd5_world --out "$out/gd5-fps.json"
fi

if command -v "$freeciv_server" >/dev/null 2>&1; then
    wants tpm && measure "Freeciv turn speed" tpm freeciv --turns "$turns" --repeat "$repeats" --timeout "$tpm_timeout" $common --freeciv-server "$freeciv_server" $freeciv_world --out "$out/freeciv-tpm.json"
    wants footprint && measure "Freeciv footprint" footprint freeciv --turns 20 $common --freeciv-server "$freeciv_server" $freeciv_world --out "$out/freeciv-footprint.json"
    wants net && measure "Freeciv network" net freeciv --turns "$net_turns" --clients "$clients" $common --freeciv-server "$freeciv_server" --freeciv-client "$freeciv_client" $freeciv_world --out "$out/freeciv-net.json"
    wants fps && measure "Freeciv frame rate" fps freeciv $common --out "$out/freeciv-fps.json"
fi

if [ -n "$unciv_jar" ]; then
    wants tpm && measure "Unciv turn speed" tpm unciv --turns "$turns" --repeat "$repeats" --timeout "$tpm_timeout" $common --unciv-jar "$unciv_jar" $java_options $unciv_world --out "$out/unciv-tpm.json"
    wants footprint && measure "Unciv footprint" footprint unciv --turns 20 $common --unciv-jar "$unciv_jar" $java_options $unciv_world --out "$out/unciv-footprint.json"
    wants net && measure "Unciv network" net unciv --turns "$net_turns" $common --unciv-jar "$unciv_jar" $java_options $unciv_world --out "$out/unciv-net.json"
    wants fps && measure "Unciv frame rate" fps unciv --turns 20 --seconds "$fps_seconds" --timeout "$fps_timeout" $common --unciv-jar "$unciv_jar" $java_options $unciv_world --out "$out/unciv-fps.json"
fi

rm -rf "$work"
"$ojh" report "$out" | tee -a "$log"
echo "results, report and graphs: $out"

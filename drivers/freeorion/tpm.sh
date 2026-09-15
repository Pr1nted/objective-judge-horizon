#!/bin/sh
set -u

server=${FREEORION_SERVER:-/Applications/FreeOrion.app/Contents/Executables/freeoriond}
stars=${FREEORION_STARS:-150}
turns=100
players=8
seed=20260914
work=.

while [ $# -gt 0 ]; do
    case $1 in
        --turns) turns=$2; shift 2 ;;
        --players) players=$2; shift 2 ;;
        --stars) stars=$2; shift 2 ;;
        --seed) seed=$2; shift 2 ;;
        --work) work=$2; shift 2 ;;
        --server) server=$2; shift 2 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

run="$work/freeorion-$$"
rm -rf "$run"
mkdir -p "$run/save" "$run/ai"
fifo="$run/log.fifo"
mkfifo "$fifo"

"$server" --hostless --load-or-quickstart \
    --network.server.human.min 0 --network.server.conn-human-empire-players.min 0 \
    --setup.ai.player.count "$players" --setup.star.count "$stars" --setup.seed "$seed" \
    --save.path "$run/save" --ai-log-dir "$run/ai" --log-file /dev/stdout > "$fifo" 2>&1 &
pid=$!

echo "OJH players $players"
echo "OJH regions $stars systems"

awk -v turns="$turns" '
    /Turn number incremented/ {
        if (!started) { print "OJH ready"; started = 1; fflush(); next }
        n++
        print "OJH turn " n
        fflush()
        if (n >= turns) exit
    }
' < "$fifo"

kill "$pid" 2>/dev/null
pkill -f "freeorionca.*$run" 2>/dev/null
wait "$pid" 2>/dev/null
sleep 1
pkill -9 -f "freeorionca.*$run" 2>/dev/null
rm -rf "$run"

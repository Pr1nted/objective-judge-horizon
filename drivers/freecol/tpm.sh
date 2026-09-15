#!/bin/sh
set -u

here=$(cd "$(dirname "$0")" && pwd)
freecol=${FREECOL_DIR:-/Applications/FreeCol}
java_bin=${JAVA_BIN:-}
europeans=${FREECOL_EUROPEANS:-8}
map=${FREECOL_MAP:-40x100}
turns=100
seed=20260914
work=.

while [ $# -gt 0 ]; do
    case $1 in
        --turns) turns=$2; shift 2 ;;
        --seed) seed=$2; shift 2 ;;
        --work) work=$2; shift 2 ;;
        --europeans) europeans=$2; shift 2 ;;
        --map) map=$2; shift 2 ;;
        *) echo "unknown option $1" >&2; exit 2 ;;
    esac
done

java=java
javac=javac
if [ -n "$java_bin" ]; then
    java="$java_bin/java"
    javac="$java_bin/javac"
fi

classpath="$freecol/FreeCol.jar"
for jar in "$freecol"/jars/*.jar; do
    classpath="$classpath:$jar"
done

classes="$work/freecol-driver"
mkdir -p "$classes"
"$javac" -nowarn -d "$classes" -cp "$classpath" "$here/FreeColTpm.java" >&2 || exit 2

width=${map%x*}
height=${map#*x}
cd "$freecol" && exec "$java" -Xmx2G -Djava.awt.headless=true -cp "$classes:$classpath" FreeColTpm "$turns" "$seed" "$europeans" "$width" "$height"

#!/usr/bin/env bash

shopt -s nullglob

MAKECMD=${MAKECMD:-make}

rebuild() {
    if [[ ! -d "$1" ]]; then
        return
    fi

    echo "==> Rebuilding modules in $1"

    for m in "$1"/*; do
        if [[ ! -d "$m" ]]; then
            continue
        fi

        echo "====> Rebuilding $(basename "$m")"

        MODULEDIR="$(dirname "$(dirname "$m")")"
        export MODULEDIR

        "$MAKECMD" -C "$m" clean > /dev/null
        "$MAKECMD" -C "$m" uninstall-so > /dev/null
        "$MAKECMD" -C "$m" > /dev/null
        "$MAKECMD" -C "$m" install-so > /dev/null
        "$MAKECMD" -C "$m" clean > /dev/null
    done
}

rebuild /usr/lib/blockbar/modules/src
rebuild /usr/local/lib/blockbar/modules/src

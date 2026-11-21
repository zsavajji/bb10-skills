#!/bin/sh
# Lints Cascades QML with Qt 6 qmllint against the BB10 NDK's own type info
# (plugins.qmltypes for bb.cascades 1.4 and the other bb.* modules).
# Catches syntax errors, duplicate ids, misspelt properties, signals, enum
# values and types. It does NOT check that an API exists in an older import
# version (bb.cascades 1.2 vs 1.4); 1.4 (OS 10.3.1) is the newest there is.
#
#   cascades-qmllint.sh assets/*.qml [more qmllint options]
#
# First run fills ${XDG_CACHE_HOME:-~/.cache}/bb10-qmllint: the type info from
# the NDK image ($BB10_NDK_IMAGE, default yamsergey/bb10-ndk:0.6) and, unless
# $QMLLINT names a Qt 6 qmllint, Debian's Qt 6 qmllint unpacked (it needs the
# system's Qt 6 Core/Qml libraries). Delete the folder to refresh it. On a machine
# without apt-get (macOS) it runs that qmllint in a Debian container instead.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
cache=${XDG_CACHE_HOME:-$HOME/.cache}/bb10-qmllint
image=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}
imports=/root/bbndk/target_10_3_1_995/qnx6/armle-v7/usr/lib/qt4/imports

if [ ! -f "$cache/imports/bb/cascades/plugins.qmltypes" ]; then
    rm -rf "$cache/imports" && mkdir -p "$cache/imports"
    docker run --rm "$image" tar -C "$imports" -cf - --exclude='*.so' bb bb.1 | tar -xf - -C "$cache/imports"
    python3 -I "$here/qmltypes6.py" "$cache/imports"
fi
# `import QtQuick 1.0` in Cascades code: its Timer and Connections.
if ! cmp -s "$here/qtquick1.qmltypes" "$cache/imports/QtQuick.1/plugins.qmltypes"; then
    mkdir -p "$cache/imports/QtQuick.1"
    printf 'module QtQuick\ntypeinfo plugins.qmltypes\n' > "$cache/imports/QtQuick.1/qmldir"
    cp "$here/qtquick1.qmltypes" "$cache/imports/QtQuick.1/plugins.qmltypes"
fi

lint=${QMLLINT:-}
# No apt-get (macOS, a non-Debian Linux): Debian's Qt 6 qmllint in a container instead. The
# cache and the current folder are mounted at their own paths, so -I and relative file
# arguments work unchanged; files outside the current folder aren't seen.
if [ -z "$lint" ] && ! command -v apt-get >/dev/null 2>&1; then
    docker build -q -t bb10-qmllint - >/dev/null <<'EOF'
FROM debian:trixie-slim@sha256:f6b08b09a05725f9431502d3b5714f5c4576e82c1a84fa1debe0c24a7e0c3ec4
RUN apt-get update && apt-get install -y --no-install-recommends qt6-declarative-dev-tools \
    && rm -rf /var/lib/apt/lists/*
EOF
    exec docker run --rm -v "$cache:$cache" -v "$PWD:$PWD" -w "$PWD" bb10-qmllint \
        /usr/lib/qt6/bin/qmllint -I "$cache/imports" --unqualified disable "$@"
fi
if [ -z "$lint" ]; then
    lint=$cache/qt6/usr/lib/qt6/bin/qmllint
    if [ ! -x "$lint" ]; then
        mkdir -p "$cache/deb" && (cd "$cache/deb" && apt-get download qt6-declarative-dev-tools libqt6qmlcompiler6)
        for deb in "$cache"/deb/*.deb; do dpkg-deb -x "$deb" "$cache/qt6"; done
    fi
    export LD_LIBRARY_PATH="$cache/qt6/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# Context properties (setContextProperty) and Cascades' Color/Qt globals read
# as unqualified names; they are expected in Cascades code.
exec "$lint" -I "$cache/imports" --unqualified disable "$@"

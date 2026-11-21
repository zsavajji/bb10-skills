#!/bin/sh
# Builds build/hello.bar (unsigned) in the BB10 Native SDK 10.3.1 image, with the next build id
# from build/buildid.txt (-buildIdFile), so the installer never mistakes a new build for the old one.
# Installing: hand the .bar to the user (Sachesi or DBBT with Development Mode on), or, only when
# the user says so, over root SSH through the installer's PPS job (bb10-build-deploy, "Install").
set -eu
repo=$(cd "$(dirname "$0")" && pwd)
image=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}
mkdir -p "$repo/build"
[ -f "$repo/build/buildid.txt" ] || echo 1 >"$repo/build/buildid.txt"

docker run --rm -v "$repo:/src" -w /src "$image" bash -c '
    source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
    set -eu
    mkdir -p build
    (cd build && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../hello.pro && make -j"$(nproc)")
    blackberry-nativepackager -package build/hello.bar -buildIdFile build/buildid.txt \
        bar-descriptor.xml -configuration Device-Release
    rm -f assets/.assets.index
'
unzip -p "$repo/build/hello.bar" META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'

#!/bin/sh
# Builds build/tengram.bar: the engine with the Telegram API id and hash compiled in (engine/build.sh), the UI
# (scripts/build-ui.sh), the memory addon (rlimit-patch, copied to build/headless-mem.zip) and the spawn helper
# tg-install-patcher (app/helper), then the package, unsigned, with all of them (app/bar-descriptor.xml). Each package
# gets the next build id from build/buildid.txt: the installer keeps the old app when Package-Version is the same.
# TG_API_ID and TG_API_HASH come from the environment, else from .tg-keys at the repo's root (gitignored, the user's).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}
if [ -f "$ROOT/.tg-keys" ]; then
	: "${TG_API_ID:=$(sed -n 's/^TG_API_ID=//p' "$ROOT/.tg-keys")}"
	: "${TG_API_HASH:=$(sed -n 's/^TG_API_HASH=//p' "$ROOT/.tg-keys")}"
	export TG_API_ID TG_API_HASH
fi
if [ -z "${TG_API_ID-}" ] || [ -z "${TG_API_HASH-}" ]; then
	echo "build-app.sh: set TG_API_ID and TG_API_HASH (my.telegram.org)" >&2
	exit 2
fi
"$ROOT/engine/build.sh" >/dev/null
"$ROOT/scripts/build-ui.sh" >/dev/null
# The app offers the headless-mem addon (app/src/MemoryPatch.cpp, docs/headless-cap.md). The docker mount below
# covers only this project, so the zip has to be a real copy.
# base-bb10: path changed (was ../rlimit-patch, a sibling checkout)
RLIMIT_PATCH=${RLIMIT_PATCH:?set RLIMIT_PATCH to a clone of https://github.com/zsavajji/authman-rlimit}
if [ ! -x "$RLIMIT_PATCH/build.sh" ]; then
	echo "build-app.sh: $RLIMIT_PATCH is missing: the package bundles its headless-mem.zip" >&2
	exit 2
fi
"$RLIMIT_PATCH/build.sh" >/dev/null
cp "$RLIMIT_PATCH/build/headless-mem.zip" "$ROOT/build/headless-mem.zip"
[ -f "$ROOT/build/buildid.txt" ] || echo 1 >"$ROOT/build/buildid.txt"
docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" bash -c '
	source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
	set -eu
	mkdir -p build/ui
	qcc -Vgcc_ntoarmv7le -O2 -o build/ui/tg-install-patcher app/helper/tg-install-patcher.c
	cd app && blackberry-nativepackager -package ../build/tengram.bar -buildIdFile ../build/buildid.txt \
		bar-descriptor.xml -configuration Device-Release
	rm -f assets/.assets.index
'
unzip -p "$ROOT/build/tengram.bar" META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'

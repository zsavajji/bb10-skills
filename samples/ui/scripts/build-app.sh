#!/bin/sh
# Builds build/phoenix.bar: the engine (scripts/build-engine.sh, skipped with SKIP_ENGINE=1 when
# build/engine-bb10/phoenixd exists), the Cascades UI in the Native SDK 10.3.1 image, and the
# unsigned package of both. Each package gets the next build id (build/app-buildid.txt).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
# The roots the engine's TLS trusts (BB10's own store is from 2013): Mozilla's bundle, fetched,
# not committed. The engine reads app/native/assets/cacert.pem.
CA=$ROOT/app/assets/cacert.pem
[ -s "$CA" ] || curl -fsSL https://curl.se/ca/cacert.pem -o "$CA"
IMAGE=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}
mkdir -p "$ROOT/build"

if [ "${SKIP_ENGINE:-0}" = 1 ] && [ -x "$ROOT/build/engine-bb10/phoenixd" ]; then
	echo "build-app.sh: using the existing build/engine-bb10/phoenixd"
else
	"$ROOT/scripts/build-engine.sh"
fi

# What a build would otherwise only show on the phone: a signal handler that does not exist
# (the app quits at start). Runs where the SDK's type information is cached.
if [ -d "$HOME/.cache/bb10-qmllint/imports" ]; then
	python3 "$ROOT/scripts/check-qml-handlers.py"
else
	echo "build-app.sh: no qmllint cache, signal handlers not checked" >&2
fi
if command -v node >/dev/null 2>&1; then node "$ROOT/app/tests/format_test.js"; fi
# base-bb10: path changed (was ../base-bb10/skills/..., a sibling checkout)
LINT=$ROOT/skills/bb10-build-deploy/scripts/cascades-qmllint.sh
if [ -f "$LINT" ]; then
	(cd "$ROOT" && sh "$LINT" app/assets/*.qml)
else
	echo "build-app.sh: $LINT not found, QML not linted" >&2
fi

[ -f "$ROOT/build/app-buildid.txt" ] || echo 1 >"$ROOT/build/app-buildid.txt"
docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" bash -c '
	source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
	set -eu
	mkdir -p build/app-ui
	# lupdate parses QML with Qt 4.8, as the phone does; a QML error quits the app at start.
	if lupdate app/assets/*.qml -ts /tmp/qml-check.ts 2>&1 | grep -A2 "error:"; then
		echo "build-app.sh: QML that Qt 4.8 cannot parse" >&2
		exit 1
	fi
	# The translations: new strings into app/translations/*.ts (the sources are in English,
	# which has no file), then compiled for the bar. No apostrophes in here: this is one
	# single-quoted string of the outer shell.
	mkdir -p app/translations build/qm
	lupdate -silent -no-obsolete app/assets/*.qml app/assets/*.js app/src/*.cpp \
		-ts app/translations/phoenix_{it,fr,de,pl,ru,hi,vi,id}.ts
	for ts in app/translations/*.ts; do
		lrelease -silent "$ts" -qm "build/qm/$(basename "${ts%.ts}").qm"
	done
	(cd build/app-ui && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../../app/phoenix.pro >/dev/null &&
		make -j"$(nproc)" >/dev/null)
	ls -l build/app-ui/app/phoenix
	cd app && blackberry-nativepackager -package ../build/phoenix.bar \
		-buildIdFile ../build/app-buildid.txt bar-descriptor.xml -configuration Device-Release
	rm -f assets/.assets.index'
unzip -p "$ROOT/build/phoenix.bar" META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'
unzip -l "$ROOT/build/phoenix.bar"

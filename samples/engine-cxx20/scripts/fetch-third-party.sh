#!/bin/sh
# Fetches the third-party sources the engine compiles, pinned by sha256, into engine/third_party/.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
YYJSON_TAG=0.13.0
YYJSON_C_SHA=d2d58ef0a3b2267862dc363832c4f3185fd375aea933d5d3419d1a940d6ece2e
YYJSON_H_SHA=ef803cda5c06b8962face6dfa39c3284b3bcf3e73f4d7317664690cc779679ca

sha() { if command -v sha256sum >/dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }

fetch() {  # dir url sha file
	dest=$1/$4
	mkdir -p "$1"
	[ -f "$dest" ] && [ "$(sha "$dest")" = "$3" ] && return 0
	curl -fsSL "$2" -o "$dest.tmp"
	if [ "$(sha "$dest.tmp")" != "$3" ]; then
		rm -f "$dest.tmp"
		echo "fetch-third-party: $4 does not match its pinned sha256" >&2
		exit 1
	fi
	mv "$dest.tmp" "$dest"
}

Y=$ROOT/engine/third_party/yyjson
base=https://raw.githubusercontent.com/ibireme/yyjson/$YYJSON_TAG/src
fetch "$Y" "$base/yyjson.c" "$YYJSON_C_SHA" yyjson.c
fetch "$Y" "$base/yyjson.h" "$YYJSON_H_SHA" yyjson.h

# libwebp: the release tarball, unpacked whole (the build picks the decoder files). The stamp is
# written last, so a half-extracted tree is fetched again.
WEBP_SHA=61f873ec69e3be1b99535634340d5bde750b2e4447caa1db9f61be3fd49ab1e5
W=$ROOT/engine/third_party/libwebp
if [ ! -f "$W/.sha256" ] || [ "$(cat "$W/.sha256")" != "$WEBP_SHA" ]; then
	rm -rf "$W"
	mkdir -p "$W"
	curl -fsSL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.4.0.tar.gz -o "$W.tar.gz"
	if [ "$(sha "$W.tar.gz")" != "$WEBP_SHA" ]; then
		rm -rf "$W.tar.gz" "$W"
		echo "fetch-third-party: libwebp does not match its pinned sha256" >&2
		exit 1
	fi
	tar -xzf "$W.tar.gz" -C "$W" --strip-components=1
	rm -f "$W.tar.gz"
	echo "$WEBP_SHA" >"$W/.sha256"
fi

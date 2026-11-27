# A Go engine (hyperberry)

Read this when: you consider Go for an engine, or you maintain/extend HyperBerry's `hyperberryd`: toolchain, build and test scripts, patching dependencies, and every Go-on-QNX trap the sources recorded.

Choosing between Go and C++20 is in [02-architecture.md](02-architecture.md) §6. The C++ route is [04-engine-cxx20.md](04-engine-cxx20.md). Only HyperBerry uses Go; its sources are the only ones for this doc.

## 1. Toolchain

| Item | Value | Source |
|---|---|---|
| Go | **go-qnx 1.27.4** (the user's fork [zsavajji/go-qnx](https://github.com/zsavajji/go-qnx) of XIAZY's), `GOOS=qnx GOARCH=arm GOARM=7` | HyperBerry's plan §2 |
| Image | go-qnx's `/usr/local/go` copied into the bb10-cxx20 image | [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go) |
| Pin | go-qnx image **by digest**: `ghcr.io/zsavajji/go-qnx:1.27.4@sha256:19f06aa6606912a3fd89c1e4eca226292a9fb1071ec57d2d09b5b3ee8529b094` | same |
| Why the digest | "go-qnx's README says to pin its image by digest: its go1.27.4 has Google's version string but not Google's bytes" | Dockerfile comment; HyperBerry's design notes §8 |
| cgo | supported on qnx/arm with an external C compiler: **`CC=bb10-clang`** (from bb10-cxx20), `CGO_ENABLED=1` | HyperBerry's plan §2 |
| `GOTOOLCHAIN` | `local` (set in the Dockerfile with `PATH=/usr/local/go/bin:$PATH`) | [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go) |
| `golang.org/x/sys` | replaced by XIAZY's fork in `engine/go.mod` | [samples/engine-go/go.mod](../samples/engine-go/go.mod) |
| Go cache | `~/.cache/hyperberry-go` on the Mac, mounted at `/cache`, "apart from stock Go's (go-qnx README)" | [samples/engine-go/build-engine.sh](../samples/engine-go/build-engine.sh) |
| Base image tag | `bb10-cxx20:19.1.7-6-build` | [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go) |

The Dockerfile, in short:

```dockerfile
FROM ghcr.io/zsavajji/go-qnx:1.27.4@sha256:19f06aa6606912a3fd89c1e4eca226292a9fb1071ec57d2d09b5b3ee8529b094 AS go
FROM bb10-cxx20:19.1.7-6-build
COPY --from=go /usr/local/go /usr/local/go
ENV PATH=/usr/local/go/bin:$PATH GOTOOLCHAIN=local
```

then libopus built twice (BB10 sysroot and host, [06-third-party-libs.md](06-third-party-libs.md)), and `apt-get install patch` "for engine/framesocket.patch".

`go.mod` ([samples/engine-go/go.mod](../samples/engine-go/go.mod)), the lines that matter:

```
module hyperberry/engine

go 1.27.2

replace golang.org/x/sys => github.com/XIAZY/go-sys-qnx v0.0.0-20261016215730-6348336c2429
```

Libraries pinned there include `github.com/mattn/go-sqlite3 v1.14.52`, `github.com/polymorfa/hypermeow v0.0.0-20261112193137-7ced3258db46` (a `main` pseudo-version "because its README retracts every tag": HyperBerry's plan §2; it needs Go >= 1.26), `github.com/purpshell/meowcaller` (calls) and `golang.org/x/crypto/x509roots/fallback`.

Go code rule: **std plus the protocol library only; cgo only for SQLite, BPS, UDS and libopus** (HyperBerry's design notes §6; `PLAN.md` §12).

### Moving from go-qnx 1.27.2 to 1.27.4 (HyperBerry, 2026-11-27)

- **`syscall.RLIM_INFINITY` is now unsigned** (`1<<64-3`, QNX's `(rlim_t)-3`); Linux's is -1. `int64(syscall.RLIM_INFINITY)`
  no longer compiles for qnx. For code built for both (host tests): `uint64(syscall.RLIM_INFINITY & (1<<64 - 1))`.
  Host tests don't catch it: only the BB10 build does.
- go-qnx's README ("Performance on BlackBerry 10", measured on a Q10, not by us): `time.Now` 326 ns from 2115;
  channel and scheduler work 11-15% faster; a heap mapped lazily (1 MB live: 3.2 MB of data from 5.7 MB); an
  allocation-heavy program about 30% faster; CPU profiles see through libc. Network speed is unchanged (io-pkt).
- HyperBerry 1.0.6.424 with it was built and installed on a Q20; memory and speed were not compared.

## 2. `scripts/build-engine.sh`, verbatim

```sh
#!/bin/sh
# Builds build/hyperberryd for BlackBerry 10 (qnx/arm, cgo through bb10-clang), then checks that
# every symbol it imports exists in the phone's libraries: a missing one links fine here and
# only fails on the phone.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/hyperberry-go   # apart from stock Go's (go-qnx README)
mkdir -p "$CACHE" "$ROOT/build"
docker build -q -t hyperberry-build "$ROOT/docker" >/dev/null
docker run --rm -v "$ROOT:/src" -v "$CACHE:/cache" -w /src/engine \
	-e GOPATH=/cache/gopath -e GOCACHE=/cache/build \
	-e CGO_ENABLED=1 -e CC=bb10-clang -e GOOS=qnx -e GOARCH=arm -e GOARM=7 \
	hyperberry-build sh -euc '
		# libunifieddatasourcec.so NEEDs the Qt libraries: only the linker looks for them here.
		export CGO_LDFLAGS="-Wl,-rpath-link,$QNX_TARGET/armle-v7/usr/lib/qt4/lib"
		work=$(sh /src/scripts/hypermeow-work.sh)
		export GOWORK=$work
		go build -trimpath -ldflags="-s -w" -o /src/build/hyperberryd .
		L=$QNX_TARGET/armle-v7/lib
		for l in $L/libc.so.3 $L/libm.so.2 $L/libsocket.so.3 $L/../usr/lib/libbps.so.3 \
				$L/../usr/lib/libunifieddatasourcec.so.1 $L/libmmrndclient.so.1 $L/../usr/lib/libstrm.so.1; do
			nm -D --defined-only $l | awk "{ print \$3 }"
		done | sed "s/@.*//" | sort -u >/tmp/defined
		missing=$(nm -D --undefined-only /src/build/hyperberryd | awk "\$1 == \"U\" { print \$2 }" |
			sed "s/@.*//" | sort -u | comm -23 - /tmp/defined)
		[ -z "$missing" ] || { echo "not on BB10: $missing"; exit 1; }
		ls -l /src/build/hyperberryd'
```

What each part does:

- **Environment**: `CGO_ENABLED=1 CC=bb10-clang GOOS=qnx GOARCH=arm GOARM=7`, `GOPATH` and `GOCACHE` under `/cache` (the host's `~/.cache/hyperberry-go`).
- **`CGO_LDFLAGS=-Wl,-rpath-link,$QNX_TARGET/armle-v7/usr/lib/qt4/lib`**: the Hub library `libunifieddatasourcec.so` NEEDs Qt libraries that exist only in the SDK's `qt4/lib`; "only the linker looks for them here" (the script's own comment).
- **`-trimpath -ldflags="-s -w"`**: the release flags; the script gives no reason beyond them.
- **The imported-symbols check**: collect the dynamic symbols that the phone's libraries define (libc, libm, libsocket, libbps, libunifieddatasourcec, libmmrndclient, libstrm), then fail if the binary imports one not in the list. Same trap as in [04-engine-cxx20.md](04-engine-cxx20.md) §7: "such a binary links here and fails only on the phone". Extend the library list when the engine links a new system library.
- **`GOWORK`** from [`hypermeow-work.sh`](../samples/engine-go/patch-deps-work.sh) (section 5).
- Gate 0.1 result: hypermeow builds unmodified (`CGO_ENABLED=0`: 14 MB); with `mattn/go-sqlite3` through bb10-clang: 15.6 MB, all 153 imports present (HyperBerry's phase 0 notes 0.1).

`push-engine.sh` copies a test build to the phone (scratch folder `/root/hyperberry`, `pathtrust !`, fresh name per build): [10-phone-access-and-install.md](10-phone-access-and-install.md).

## 3. Tests on the PC (`scripts/test-engine.sh`)

```sh
#!/bin/sh
# Runs the engine's tests on the PC, in the build image. The engine is plain Go, so it also
# builds for Linux; only the *_qnx.go files and the phone itself need the BB10 build.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/hyperberry-go   # apart from stock Go's (go-qnx README)
mkdir -p "$CACHE"
docker build -q -t hyperberry-build "$ROOT/docker" >/dev/null
docker run --rm -v "$ROOT:/src" -v "$CACHE:/cache" -w /src/engine \
	-e GOPATH=/cache/gopath -e GOCACHE=/cache/build \
	-e CGO_ENABLED=1 -e CC=cc \
	hyperberry-build sh -euc '
		work=$(sh /src/scripts/hypermeow-work.sh)
		export GOWORK=$work
		go vet ./... && go test ./...'
```

- **Same image, `CC=cc`** (the image's host compiler) and no `GOOS`: the engine is plain Go, so it also builds for Linux with the same go-qnx toolchain. Most of Phase 1 ran this way, against a **simulated account** (`-sim` or a file `data/sim`, `sim*.go`, driven by [`scripts/sim.py`](../samples/phone/sim-driver.py)), and the protocol is tested with `socat - TCP:127.0.0.1:<port>` and typed JSON (`design.md` §8).
- **`*_qnx.go` for phone-only code** with `//go:build qnx` and a matching `*_other.go` stub for the PC: `navigator_qnx.go`/`navigator_other.go`, [`hub_qnx.go`](../samples/engine-go/hub_qnx.go)/[`hub_other.go`](../samples/engine-go/hub_other.go), [`net_qnx.go`](../samples/engine-go/net_qnx.go)/[`net_other.go`](../samples/engine-go/net_other.go), [`ringtone_qnx.go`](../samples/engine-go/ringtone_qnx.go)/[`ringtone_other.go`](../samples/engine-go/ringtone_other.go) (HyperBerry's engine). These use cgo with `#cgo LDFLAGS: -lbps`.
- Pitfall seen with the tests: a request field of the same name and another type as one in the server's envelope fails the whole request as `bad_request`; handler-level tests (`r.raw`) did not see it. **Test a new field through the socket** (`pitfalls.md`, "Go on BB10").

## 4. Memory under the headless cap

- Headless entry point: **3 MB** heap without a memory class; the engine "exited 47 ms after launch, before `main`", and the UI's invoke got error 6 (`Target`). `_sys_headless_xlarge` lifts it (30 MiB; 128 MiB with `rlimit-patch`). Idle and unlinked the engine held 6.4 MB (HyperBerry's phase 0 notes 0.3; `rlimit-patch/README.md`). All the headless rules: [09-headless-service.md](09-headless-service.md).
- **The runtime writes one byte to every heap and stack page before use**: 1.7 ms per MB on BB10, once per page (go-qnx README, in HyperBerry's plan §2).
- **Out of memory is not recoverable in Go**, and an allocation other platforms shrug off kills the engine: hypermeow believed a frame's claimed length (fixed by [`engine/framesocket.patch`](../samples/engine-go/patches/framesocket.patch)) (`pitfalls.md`).
- **Knobs**: `debug.SetMemoryLimit` and a lower `GOGC` "before anything structural" (`design.md` §6). [`main.go`](../samples/engine-go/main.go) calls `rdebug.SetMemoryLimit(goMemLimit(memMB))`; the limit "follows the real cap (four fifths of it)" (HyperBerry's translation notes). It was `SetMemoryLimit(24 MB)` before the memory patch (HyperBerry's phase 7 notes).
- **Design to the cap**: whole files and pictures in memory cannot work. Files stream (downloads through `DownloadToFile`, uploads through `UploadReader` with a temporary file); **the engine never decodes a picture**: the UI makes photo previews with Qt's JPEG reader into `data/cache/previews/` (HyperBerry's phase 6 notes). Two real 30 MiB failures were bugs fixed at the source, not cap problems: a voice note decoded whole (31 MB) and a group list fetched with its members (`translate.md`).
- **Calls audio runs in the UI process** because of the capped heap: "the microphone, the speaker and their buffers belong to the UI process, which has no such cap. The engine keeps what only Go can do: meowcaller's signalling, SRTP and the MLOW codec" (`phase7.md`, the user's calls, 2026-11-07). Contrast: bbmusic's engine owns the speaker (C++ heap, no GC).
- **`crash.log`**: keeps old crashes, and an out-of-memory dump's first stack is who asked last, not who took the memory. Grep a dump for every `main.` frame and read `git log` around its date before blaming (`pitfalls.md`).

## 5. Patching dependencies without forking: [`scripts/hypermeow-work.sh`](../samples/engine-go/patch-deps-work.sh)

```sh
#!/bin/sh
# Prints the path of a go.work that swaps hypermeow for a copy with engine/framesocket.patch,
# engine/hypermeow-status.patch and engine/hypermeow-fullsync.patch applied, and meowcaller for one with
# engine/meowcaller.patch (`go -overlay` refuses files in the module cache). Runs in the build image,
# from /src/engine. A module whose file has moved on makes patch fail, which stops the build instead
# of losing the fix.
set -eu
go mod download github.com/polymorfa/hypermeow
src=$(go list -m -f '{{.Dir}}' github.com/polymorfa/hypermeow)
rm -rf /tmp/hypermeow
cp -r "$src" /tmp/hypermeow
chmod -R u+w /tmp/hypermeow
patch -s --fuzz=0 -p1 -d /tmp/hypermeow </src/engine/framesocket.patch >&2
patch -s --fuzz=0 -p1 -d /tmp/hypermeow </src/engine/hypermeow-status.patch >&2
patch -s --fuzz=0 -p1 -d /tmp/hypermeow </src/engine/hypermeow-fullsync.patch >&2
go mod download github.com/purpshell/meowcaller
src=$(go list -m -f '{{.Dir}}' github.com/purpshell/meowcaller)
rm -rf /tmp/meowcaller
cp -r "$src" /tmp/meowcaller
chmod -R u+w /tmp/meowcaller
# whatsapp-light's fixes first (engine/meowcaller/*.patch, in order: phase7b.md), then ours: the FFT
# one, the group calls', and the peer's bandwidth allowance (PRST).
for p in /src/engine/meowcaller/*.patch; do
	patch -s --fuzz=0 -p1 -d /tmp/meowcaller <"$p" >&2
done
patch -s --fuzz=0 -p1 -d /tmp/meowcaller </src/engine/meowcaller.patch >&2
patch -s --fuzz=0 -p1 -d /tmp/meowcaller </src/engine/meowcaller-group.patch >&2
patch -s --fuzz=0 -p1 -d /tmp/meowcaller </src/engine/meowcaller-prst.patch >&2
cat >/tmp/go.work <<EOF
go $(go list -m -f '{{.GoVersion}}')
use /src/engine
replace github.com/polymorfa/hypermeow => /tmp/hypermeow
replace github.com/purpshell/meowcaller => /tmp/meowcaller
EOF
echo /tmp/go.work
```

The recipe, for any dependency you must patch:

1. `go mod download <module>`, then copy the module out of the module cache (`go list -m -f '{{.Dir}}'`) to a scratch folder and `chmod -R u+w` it (the cache is read-only).
2. Apply your patches with **`patch -s --fuzz=0 -p1`**. Zero fuzz on purpose: "A module whose file has moved on makes patch fail, which stops the build instead of losing the fix."
3. Write a **generated `go.work`** that `use`s the engine and `replace`s the module with the patched copy; print its path; the build exports `GOWORK=$work`.
4. **Why not `go -overlay`**: it "refuses files in the module cache".
5. The patch files live in `engine/` ([`framesocket.patch`](../samples/engine-go/patches/framesocket.patch), [`hypermeow-status.patch`](../samples/engine-go/patches/hypermeow-status.patch), [`hypermeow-fullsync.patch`](../samples/engine-go/patches/hypermeow-fullsync.patch), `meowcaller*.patch`, `meowcaller/0N-*.patch`) and are applied at build time. Bumping the dependency may stop the build: that is the intent.

## 6. Go-specific findings (HyperBerry's pitfalls notes "Go on BB10", in full, with the other recorded ones)

All tagged [Q10] (seen on the user's Q10) unless noted.

| Trap | Fix / fact |
|---|---|
| A process that exits while binding a **named Unix socket** crashes **io-pkt** (the phone's network stack) until a reboot; reproduced on BB10. Relative socket paths resolve from `/` | The UI link is loopback TCP; use absolute paths everywhere (§2 of the plan) |
| **BB10 has no CA store where Go looks** (go-qnx's `root_qnx.go` reads `/etc/openssl`, which BB10 does not have): TLS failed with "certificate signed by unknown authority" (2026-10-28) | `engine/roots.go`: `//go:debug x509usefallbackroots=1` and `import _ "golang.org/x/crypto/x509roots/fallback"`: Mozilla's roots, used even on a phone with its own store, so every phone trusts the same set. They age with `golang.org/x/crypto`: bump it with hypermeow |
| **`modernc.org/sqlite` has no qnx** (`modernc.org/libc`); meowcaller's own CLI example fails on it | cgo **`mattn/go-sqlite3`** with the bundled amalgamation (engine SQLite 3.53.4 vs the phone's 3.8.7.1 for the UI's reader: two processes, different versions, tested in gate 0.4) |
| The runtime writes **one byte to every new page**, 1.7 ms per MB | allocate less; see section 4 |
| **No runtime CPU detection on QNX**: libopus builds with `--disable-rtcd` and `-mfpu=neon`, or the ARM build does not compile | fixed at configure time (gate 0.8) |
| A goroutine reading a tunable global races a test that restores it | copy the value before `go` |
| An allocation other platforms shrug off **kills the engine** (out of memory is not recoverable; hypermeow believed a frame's claimed length) | [`engine/framesocket.patch`](../samples/engine-go/patches/framesocket.patch) |
| **Every request goes through the server's envelope first**, a struct with typed fields (`@`, `cmd`, `token`, `audio`, `video`): a request field of the same name and another type fails the whole request as `bad_request` (`call.answer`'s bool `video` did) | test new fields through the socket |
| **Google's keyless translate endpoint refuses Go's HTTP/2 client**: 429 at the first request, whatever the User-Agent; HTTP/1.1 gets 200 | a transport with an empty `TLSNextProto`, made fresh: a clone of `http.DefaultTransport` already offers h2 in its TLS config ([`engine/translate.go`](../samples/engine-go/translate.go)) |
| **gofmt rewrites `''` in a doc comment as a curly quote** | write "empty" |
| **Docker can show a file just rewritten on the Mac at its old length** (gofmt saw [`translate.go`](../samples/engine-go/translate.go) cut short) | run the check again before believing it |
| Timers have **1 ms granularity** (go-qnx README) | do not rely on finer |
| **Passport Wi-Fi** corrupts about one frame in thousands when several connections send at once; the Q10's different Wi-Fi is not known to be affected | TLS everywhere, retry on "bad record MAC" (`PLAN.md` §2) |
| A **headless app's Hub account**: the UDS C library works from a Go engine with no Qt event loop, in synchronous mode (cgo to `libunifieddatasourcec`, [`hub_qnx.go`](../samples/engine-go/hub_qnx.go)) | [09-headless-service.md](09-headless-service.md) |
| BPS from Go: `navigator_qnx.go` (cgo, `-lbps`) connects to the navigator first thing (366 ms after launch; the rule is 6 s), hands each invoke's action to the engine, and turns `NAVIGATOR_EXIT` (sent before a reinstall, with 3 s to go) into a clean shutdown | gate 0.3 |
| Notifications from the engine: BPS `notify`/`withdraw` initialise BPS on the calling thread, so any goroutine can post; they work only from an app, not from a root shell | gate 0.5 |
| Pitfall of the test harness: the **gate pages invoke the engine with actions** (`com.zsavajji.hyperberry.SQLTEST`) because a headless launch gets no arguments | gate 0.4 |

## 7. When Go is the wrong choice (BBMusic's plan §3 "Why C++ and not Go")

- Go's gains (TLS 1.3, an HTTP client, JSON, development on the Mac) are available in C++ from wolfSSL or OpenSSL, libcurl and yyjson; the Mac loop belongs to "an engine with no Qt in it", not to Go.
- Go's costs: a **15.6 MB binary** (HyperBerry's, measured) against 3-4 MB for a C++ engine (the gate 0.3 probe with libcurl, wolfSSL and the C++ runtime is 2.4 MB stripped); a **garbage-collected heap inside a headless memory class**, where running out kills the process; **cgo** for libopus, libasound, BPS and SQLite; a dependency on the go-qnx fork.
- Tengram: "a Go runtime would eat the margin" under the 30 MiB cap, and TDLib is C++ (Tengram's notes).
- C++ costs: parsers of network bytes without memory safety. Mitigation: ASan/UBSan on the Mac and fuzz targets ([04-engine-cxx20.md](04-engine-cxx20.md)).
- **Choose Go** only when the protocol library you need is Go (HyperBerry: hypermeow, a whatsmeow fork). There is no Go library in bbmusic, tengram or phoenix.

## 8. The engine's files (HyperBerry's design notes §6)

One Go file per job, no interface with a single implementation: [`main.go`](../samples/engine-go/main.go)/[`engine.go`](../samples/engine-go/engine.go) (flags, data dir; `-sim`), [`log.go`](../samples/engine-go/log.go) (`data/logs/hyperberryd.log`, 256 KiB, one rotation), `roots.go`, `navigator_qnx.go`, [`server.go`](../samples/engine-go/server.go) (loopback socket, token, framing), [`session.go`](../samples/engine-go/session.go) (the hypermeow client and link state), [`db.go`](../samples/engine-go/db.go) (migrations), [`store.go`](../samples/engine-go/store.go) (events to rows, one writer), [`send.go`](../samples/engine-go/send.go), [`media.go`](../samples/engine-go/media.go), `voice*.go` (WAV <-> OGG Opus, libopus through cgo, Ogg pages in Go), [`notifier.go`](../samples/engine-go/notifier.go)/`hub*.go` (Hub account), `sim*.go` (the simulated account). The handshake with the UI is in [02-architecture.md](02-architecture.md) §3.

## Samples

- [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go)
- [samples/engine-go/go.mod](../samples/engine-go/go.mod)
- [samples/engine-go/build-engine.sh](../samples/engine-go/build-engine.sh)
- [samples/engine-go/roots.go](../samples/engine-go/roots.go)
- [samples/engine-go/navigator_qnx.go](../samples/engine-go/navigator_qnx.go)
- [samples/engine-go/test-engine.sh](../samples/engine-go/test-engine.sh)
- [samples/engine-go/patch-deps-work.sh](../samples/engine-go/patch-deps-work.sh)
- [samples/phone/push-engine.sh](../samples/phone/push-engine.sh)

`samples/engine-go/patch-deps-work.sh` is [`hypermeow-work.sh`](../samples/engine-go/patch-deps-work.sh) in HyperBerry (section 5). [whatsapp-light](https://github.com/vbbot/whatsapp-light): the `meowcaller/*.patch` fixes are applied first in it.

# Architecture: one .bar, two processes

Read this when: you design a new BB10 app and must decide how the UI and the engine split, how they talk, who owns the data, which language the engine uses, and how the repo is laid out.

The facts about the phone that this doc assumes are in [01-target-and-rules.md](01-target-and-rules.md).

## 1. The pattern all four apps share

A Cascades UI (Qt 4.8, C++/QML, built by the NDK's `qcc`) plus an **engine**: a separate process with no Qt in it. Both ship in one `.bar` and run in one sandbox (same uid, same `data/`).

| App | UI | Engine | Engine start | Link |
|---|---|---|---|---|
| hyperberry (WhatsApp) | Cascades, qcc | Go (go-qnx) `hyperberryd` | headless entry point, started by an invoke | loopback TCP + token |
| tengram (Telegram) | Cascades, qcc (fork of hyperberry's UI) | C++20 `tengramd` | headless entry point | loopback TCP + token |
| bbmusic (YouTube Music) | Cascades, qcc | C++20 `bbmusicd` | headless entry point | loopback TCP + token |
| phoenix (Reddit) | Cascades, qcc | C++20 `phoenixd` | **plain child of the UI** (`QProcess`), not an entry point | **the child's stdin/stdout** |

Sources: HyperBerry's design notes §3; Tengram's handoff notes §1; BBMusic's plan §1, §3; Phoenix's protocol notes, [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml), [samples/engine-link/stdio/EngineLink.cpp](../samples/engine-link/stdio/EngineLink.cpp).

Note: phoenix does not use loopback TCP. Its protocol is the same NDJSON, carried on the child's pipes.

```
                          one .bar, one sandbox (same uid, same data/)
 ┌──────────────────────────────────┐             ┌──────────────────────────────────────────┐
 │ bbmusic  (UI, Cascades/QML)      │             │ bbmusicd  (engine, headless entry point) │
 │ qcc 4.6.3 · Qt 4.8               │  127.0.0.1  │ C++20 · bb10-clang++ · static libc++     │
 │                                  │  TCP+token  │                                          │
 │ EngineLink (QTcpSocket, NDJSON) ─┼─────────────┼─► server: commands in, events out        │
 │ QML pages ◄─► C++ models         │             │   innertube ─► stream (resolve,          │
 │ Active Frame                     │             │     DASH | SABR/UMP) ─► segment cache    │
 │ WebView sign-in                  │             │   player: queue, WebM demux, libopus,    │
 │                                  │             │     equalizer, gapless ─► libasound      │
 │                                  │             │   now-playing: PPS, the overlay's buttons│
 │                                  │             │   store: SQLite (data/music.db)          │
 │                                  │             │   net: libcurl + wolfSSL + roots         │
 └──────────────────────────────────┘             └──────────────────────────────────────────┘
```
(BBMusic's plan §3. HyperBerry's diagram, HyperBerry's design notes §3, has the same shape with the engine writing `data/chats.db` and the UI reading it: "read" arrow from `ChatListModel`/`MessageModel` to `chats.db`, "only the engine writes".)

## 2. Why the engine is its own process

From HyperBerry's design notes §3, BBMusic's plan §3, Tengram's notes, [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README:

- **ABI.** Qt 4.8 and Cascades on the phone are built with GCC 4.6 against the SDK's Dinkumware C++ library. libc++ (bb10-cxx20) and Dinkumware have different `std::` ABIs and exception machinery. Never pass `std::string`, containers or exceptions between a qcc module and a libc++ module ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README "Not for Cascades/Qt code"; [skills/bb10-cxx20](../skills/bb10-cxx20/SKILL.md)). A process boundary costs one socket. A libc++ library inside a Cascades process built with GCC 4.6 was a risk of an early BBMusic draft that the process split removed (BBMusic's plan §3).
- **Go cannot sensibly live inside the Cascades process**: Go installs its own signal handlers and manages its own threads (HyperBerry's design notes §3).
- **The connection (or the speaker) must outlive the UI.** Messages and notifications must keep arriving after the user closes the app; music must go on after the app is swiped away. BB10's way is a headless entry point in the same `.bar` (`design.md` §3; BBMusic's plan §3 and §14 answer 5). Details: [09-headless-service.md](09-headless-service.md).
- **Crash isolation.** A UI crash does not drop the WhatsApp connection, and the reverse (`design.md` §3).
- **The engine is testable on the Mac** (ASan/UBSan host build for C++, plain `go test` for Go): "The Mac loop belongs to 'an engine with no Qt in it', not to Go" (BBMusic's plan §3).

### Why loopback TCP and not a Unix socket (HyperBerry's design notes §3)

- A named Unix socket risks the **io-pkt crash**: a process that exits or is killed while binding one crashes the phone's network stack until a reboot (reproduced on BB10; HyperBerry's plan §2, from go-qnx's README). Relative Unix-socket paths also resolve from `/`.
- `QTcpSocket` to 127.0.0.1 works with no extra permission.
- **Any app on the phone can reach loopback**, so the engine requires a token that only this app's sandbox can read.

## 3. The link: handshake and framing, as the code does it

### 3.1 Loopback TCP with token (hyperberry, tengram, bbmusic)

Engine side, as [samples/engine-cxx20/server.cpp](../samples/engine-cxx20/server.cpp) writes it ([samples/engine-cxx20/server.cpp](../samples/engine-cxx20/server.cpp) is the same idea):

1. Make a **128-bit token**: 16 bytes from `/dev/urandom`, hex-encoded to 32 characters (`make_token`). New at every start.
2. `socket(AF_INET)`, bind to `INADDR_LOOPBACK` with **port 0** (the system picks), `listen`, `getsockname` for the port.
3. Write `<data dir>/engine.json`, mode **0600**: write `engine.json.tmp` with `O_CREAT|O_TRUNC`, `fchmod 0600`, then `rename` into place. Content as bbmusic writes it:

```
{"port":41234,"token":"<32 hex characters>"}
```
(tengram writes `{"port": %d, "token": "%s"}` with a space after each colon: [samples/engine-cxx20/server.cpp](../samples/engine-cxx20/server.cpp).) The token is compared in constant time (`token_equal`).
4. On a clean stop the engine removes `engine.json` (`Server::stop` in both). tengram also takes a file lock `engine.lock` so a second engine on the same data folder cannot replace `engine.json` ([samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)).

UI side ([samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp), Tengram and HyperBerry are the same shape):

1. Read `QDir::homePath() + "/engine.json"` (the sandbox's `data/`). Parse `port` and `token`.
2. If the file is missing or the connection is refused, the engine is not running: start it ([section 4](#4-two-ways-to-start-the-engine)) and retry (bbmusic: quick attempts every 300 ms for 5 s, then every 5 s; "engine not up after 10 s" marks it down).
3. `QTcpSocket::connectToHost(QHostAddress::LocalHost, port)`; on connect, the first line **must** be `hello` with the token:

```
{"@":"h","cmd":"hello","token":"<token>","proto":1}
```
([samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp) `socketConnected`). Another command first, or a wrong token, gets an `auth` error and the connection closes; a wrong `proto` gets `proto` and closes ([samples/engine-cxx20/protocol-example.md](../samples/engine-cxx20/protocol-example.md), HyperBerry's protocol notes).

### 3.2 Framing (all four apps)

- One JSON object per line, UTF-8, ending in `\n`. A line is at most **1 MiB** (bbmusic additionally closes a connection whose line grows past 4 MiB without a newline).
- **Requests** (UI to engine): `"@"` (a string id the UI chooses) and `"cmd"`, plus own fields.
- **Replies** echo the id: `{"@":"7","ok":true}` with `"result":{...}`, or `{"@":"7","ok":false,"error":{"code":"...","message":"..."}}`. Switch on `code`; `message` is English for logs.
- **Events** (engine to UI): `"ev"` and no `"@"`. Every authenticated connection gets every event.
- Backpressure: a client that stops reading is closed once 256 lines are queued for it; the UI reconnects and asks for the current state (bbmusic) or re-reads `chats.db` (hyperberry: "events only say which rows to re-read").
- Ids are strings everywhere; never parse them as numbers (JS loses precision past 2^53).
- Reply order: "always in request order on one connection" (hyperberry, bbmusic). Phoenix differs: replies "can arrive in any order", commands are handled by several workers (Phoenix's protocol notes).
- UI gotcha recorded in all three `EngineLink.cpp`: QML numbers reach C++ as doubles and `JsonDataAccess` writes them as `1.0`, which Go's integer fields refuse; `EngineLink::send` turns whole numbers into integers (`normalized()`), and strips the `\n`/`\r` that `JsonDataAccess` may indent with (HyperBerry's pitfalls notes; [samples/engine-link/stdio/EngineLink.cpp](../samples/engine-link/stdio/EngineLink.cpp)).
- The UI parses with `bb::data::JsonDataAccess` (`-lbbdata`).

Write the contract in `docs/protocol.md` from the engine's code and keep it in step with the code ("When the code changes, this file changes with it": all four protocol docs).

### 3.3 Child stdio (phoenix)

The UI starts `app/native/phoenixd --data <data dir>` with `QProcess` and talks over the child's stdin and stdout. **stdout is the protocol; logs go to a file** ([samples/engine-cxx20/main-child.cpp](../samples/engine-cxx20/main-child.cpp): `<data>/phoenixd.log`). The engine exits when its stdin closes; the UI's destructor closes the write channel, waits 500 ms, then kills ([samples/engine-link/stdio/EngineLink.cpp](../samples/engine-link/stdio/EngineLink.cpp)). Right after `started()` the UI sends a `status` command and counts the link as connected when its reply arrives. No token, no port, no `engine.json`: nothing to guess. The child's stderr goes to `<home>/phoenixd.stderr`.

Trap in the same file: Cascades leaves its navigator, GPU and screen descriptors open across `exec`. Before `QProcess::start`, mark every fd from 3 up `FD_CLOEXEC` or the engine inherits them.

## 4. Two ways to start the engine

| | (a) plain child of the UI | (b) headless service entry point |
|---|---|---|
| Used by | phoenix | hyperberry, tengram, bbmusic |
| How | `QProcess::start(applicationDirPath + "/phoenixd")`; the engine is an `Qnx/Elf` asset that is **not** an entry point | a second `entryPoint` in `bar-descriptor.xml` with `application.headless` invoke target; the UI sends `InvokeRequest` with target `com.zsavajji.<app>.service`, action `com.zsavajji.<app>.START` |
| Lives while | the UI lives | after the UI is closed |
| Needs | nothing special | `_sys_run_headless`, `_sys_headless_nostop`, `_sys_headless_xlarge`; connect to the navigator within 6 s; token because of loopback |
| Can run when the UI is in the background | only while the app is minimised and holds `run_when_backgrounded` | yes |

Sources: [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml) ("The engine: not an entry point, a plain child of the UI"), [samples/descriptor/bar-descriptor-headless.xml](../samples/descriptor/bar-descriptor-headless.xml), [samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp) `startEngine`, HyperBerry's design notes §3 ("If headless doesn't hold").

**Choose (a)** when nothing has to happen while the app is closed: a client that only fetches on demand (phoenix). Fewer moving parts: no headless memory class, no navigator handshake, no token, no `engine.json`.

**Choose (b)** when the app must keep a connection, play audio or post notifications with the UI closed (messengers, music). The rules of headless apps (3 MB heap without a class, 6 s to reach the navigator, no boot start, invoke refused while the app is in the background, the memory-cap patch) are all in [09-headless-service.md](09-headless-service.md).

Fallback if headless does not hold, recorded in the HyperBerry plan: run the engine as a child of the UI and declare `run_when_backgrounded`: messages keep coming while the app is minimised, not after it is closed (HyperBerry's design notes §3).

Whatever the route, the engine must accept and ignore the arguments the navigator adds (the package name, `invoke://localhost`): bbmusic 0.1.0.3 took them for a mistake and exited in 80 ms; tengram 0.1.0.2 took a stray argument for the data folder and exited unseen in 60 ms ([samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp), [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)). Both apps default the data folder to `$HOME` (the sandbox's `data/`) and take `--data DIR` for a test run outside the sandbox. Tengram: with no `--data` the engine **is** the packaged service, so a test run always passes `--data` (Tengram's handoff notes §3).

## 5. Who owns the data

| App | Database | Who opens it | Why |
|---|---|---|---|
| hyperberry | `data/chats.db` (WAL) + `session.db` | engine writes; **UI opens `chats.db` read-only and reads it directly**; `session.db` never leaves the engine | the chat list and last messages show at once on a cold start, paging back is a local query, the socket carries only commands and "these rows changed" events (`design.md` "Why the UI reads the database itself") |
| tengram | `data/chats.db`, **SQLCipher-encrypted** (key in `db.key`) | engine writes; UI reads through Qt 4.8.6's SQLite driver **renamed `QSQLCipherDriver`** because the phone's QtSql already has a `QSQLiteDriver` | same as hyperberry; `scripts/fetch-qtsqlcipher.sh`, [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro) |
| bbmusic | `data/music.db` | **engine only**; the UI asks for rows over the socket | "one owner of the schema"; add direct reads only if a cold start feels slow (BBMusic's plan §3 "Data") |
| phoenix | none read by the UI | engine | the UI is a thin client over the protocol |

Trade-off, as HyperBerry's design notes states it:

- Engine-only: one owner of the schema, but every screen is a socket round trip and nothing shows while the engine is still starting (for example after a reboot).
- UI reads directly: fast cold start, local paging; the cost is **a schema shared by the engine's language and C++**, kept in one place, and SQLite opened by two processes. Gate 0.4 proved the second on the phone (7200 writes in 6 min, 2994 reads, 0 busy or locked; the UI runs under the engine's uid so it can use the `-shm` file; open read-only with `QSQLITE_OPEN_READONLY`: HyperBerry's phase 0 notes 0.4).
- If you do share a database: only one process writes; the message store is migrated with `PRAGMA user_version`, never rebuilt, when it is the only copy of the data (HyperBerry's plan §12). Tengram's `chats.db` is the opposite case: a cache the engine can rebuild from TDLib (Tengram's notes "What changes", History).

## 6. Choosing the engine language

| | C++20 (bb10-clang++) | Go (go-qnx) |
|---|---|---|
| Used by | tengram, bbmusic, phoenix | hyperberry |
| Reason recorded | tengram: "TDLib is C++"; "a Go runtime would eat the margin" under the headless cap (Tengram's notes Decisions, Risks 2). bbmusic: "there is no Go library we need" | hyperberry's protocol library (hypermeow/whatsmeow) is Go (BBMusic's plan §3) |
| Binary size | bbmusic gate probe with libcurl, wolfSSL and the C++ runtime: 3.0 MB, 2.4 MB stripped. Tengram: 37 MB static with TDLib | hyperberry: 15.6 MB (with SQLite via cgo); 14 MB with `CGO_ENABLED=0` (HyperBerry's phase 0 notes 0.1) |
| Memory under the headless cap | plain malloc heap; ASan'd on the Mac | a garbage-collected heap inside a headless memory class "where running out kills the process" (BBMusic's plan §3). Hyperberry: out of memory is not recoverable in Go; the runtime also writes one byte to every new page (1.7 ms per MB); `SetMemoryLimit(24 MB)` was set in [`main.go`](../samples/engine-go/main.go) and later made to follow the real cap (four fifths of it) |
| Native APIs (libasound, BPS, SQLite) | direct calls | cgo for each (libopus, libasound, BPS, SQLite) |
| Dependency risk | none on a fork | "a dependency on the go-qnx fork" |
| Memory safety | parsers of network bytes have none: run every test under ASan/UBSan on the Mac, fuzz the binary parsers | yes |
| Dev loop on the Mac | `scripts/test-engine.sh` (ASan/UBSan) | `go test` in the same image |

Go is the right choice when the protocol library you need is Go and nothing else is in the way; C++20 otherwise (BBMusic's plan §3 "Why C++ and not Go"). Build details: [04-engine-cxx20.md](04-engine-cxx20.md), [05-engine-go.md](05-engine-go.md).

## 7. The repo layout of the newest apps (phoenix, bbmusic)

```
<app>/
├── app/                      Cascades UI
│   ├── <name>.pro            qmake project
│   ├── bar-descriptor.xml
│   ├── src/                  C++ (main.cpp, EngineLink.cpp, ...)
│   ├── assets/               QML, JS, images; cacert.pem is fetched, git-ignored
│   └── translations/         <name>_it.ts ...
├── engine/
│   ├── CMakeLists.txt
│   ├── src/<module>/         one static library per module (+ src/main.cpp)
│   ├── tests/<module>/*_test.cpp   one program each
│   └── third_party/          fetched, pinned, git-ignored
├── docker/Dockerfile         bb10-cxx20 + the libraries the engine links
├── scripts/                  build-engine.sh, build-app.sh, test-engine.sh, fetch-third-party.sh,
│                             install-phone.sh, check-qml-handlers.py, gen-icons.py
├── docs/protocol.md          UI <-> engine contract
└── build/                    git-ignored output
```

[samples/engine-cxx20/gitignore](../samples/engine-cxx20/gitignore), verbatim:

```
build/
.reddit-keys
engine/third_party/yyjson/
app/assets/cacert.pem
.DS_Store
engine/tests/fixtures/mp4/*.mp4
engine/third_party/libwebp/
```

Rule behind it: **fetched sources and secrets are never committed** ([samples/engine-cxx20/scripts/fetch-third-party.sh](../samples/engine-cxx20/scripts/fetch-third-party.sh), HyperBerry's plan §12). A key file is read by `build-app.sh` and the keys are compiled in with `-D`, never written to a tracked file ([04-engine-cxx20.md](04-engine-cxx20.md)). HyperBerry also has `lib/` for fetched UI libraries (`lib/qrcodegen`, `lib/libwebp`, `lib/omxil`) and `debug/` for screenshots (never committed) (HyperBerry's design notes §8 "Repo layout").

Module layout seen in the engines: phoenix has `util net text mp4 reddit media setup core`; bbmusic has `util net proto stream media audio innertube player store platform server engine` (the `platform` module is BB10-only code: navigator, now-playing, Bluetooth routing; its sources are empty on the host via `#ifdef __QNX__`). The dependency table lives in each `engine/CMakeLists.txt` ([04-engine-cxx20.md](04-engine-cxx20.md)).

How the UI half is built and packaged: [07-ui-build.md](07-ui-build.md), [08-packaging.md](08-packaging.md).

## 8. Gates first

HyperBerry and BBMusic each ran a **Phase 0 of gates** that proved the risky parts on the phone before any app code. "A failed gate changes the plan, not the code" (BBMusic's plan §9). Use the same order for a new app: cheapest first, each gate with a pass condition, the result written down with its evidence label.

HyperBerry (HyperBerry's phase 0 notes):

| Gate | What it proved | What it taught |
|---|---|---|
| 0.1 Toolchain | the engine builds with go-qnx + cgo through bb10-clang; imports all exist on the phone | the import check script (a missing import links fine and fails on the phone) |
| 0.2 Protocol on the phone | TLS reaches the service | BB10 has no CA store where Go looks: bundle Mozilla's roots |
| 0.3 Engine as headless service | 30 min alive, started by the app | the 3 MB heap without a memory class; the 6 s navigator rule; the boot filter also fires after an install; root cannot execute an installed app's binaries, test through the navigator; no boot start (user, 2026-10-29) |
| 0.4 SQLite between two processes | 0 busy/locked in 5 min | phone SQLite 3.8.7.1; WAL file; same uid shares the `-shm` |
| 0.5 Notifications from the engine | Hub item, LED, tap invokes the UI | posts only from an app, not a root shell; item ids allow only letters, digits, `_` |
| 0.6 Call codec speed | can wait | |
| 0.7 Emoji in Cascades text | a derived font via one `FontFaceRule` | system emoji stop at Unicode 6; a full font costs 177 MB and 4-12 s stalls; measure before choosing |
| 0.8 Voice-note audio path | record, encode, decode, play | `AudioRecorder` writes 48 kHz stereo 16-bit WAV; encode at 0.117x real time; libopus needs `--disable-rtcd -mfpu=neon` |

BBMusic (BBMusic's phase 0 notes, BBMusic's plan §9):

| Gate | Where | What it proved | What it taught |
|---|---|---|---|
| 0.1 `/player` per client | Mac | which client gives a direct stream URL without a token | the answer changes with time; keep the matrix |
| 0.2 both fetchers | Mac | DASH and SABR give byte-identical files | field-level protocol details found before porting |
| 0.3 C++20 engine reaches the service | Q10 | exceptions, `std::thread`, libcurl + wolfSSL, TLS 1.3 | **`-lz` broke C++ exceptions** (fixed since in bb10-cxx20 v19.1.7-4: [04-engine-cxx20.md](04-engine-cxx20.md) section 3); curl's CMake has no `HTTP_ONLY`, each protocol is a `CURL_DISABLE_*`; test files named `<program>-*` are deleted by a push script |
| 0.4 Opus through libasound | Q10 | 600 s, 0 underruns, 5.8 % of a core | no libm call per sample (integer multiply: 5.7 % vs 18.6 %); a headless entry point may open the speaker and outlives the UI |
| 0.5 who answers the controls | Q10 | the volume overlay's buttons reach a plain process with no Qt | now-playing moves into the engine (PPS `/pps/services/multimedia/mediaplayer/control`) and survives the app closing; the overlay draws the state it is told |
| 0.6 routing and interruptions | Q10 | headphones: passed | a stream stays on the output it was opened on; the engine must watch the device; phone call and Bluetooth untested |
| 0.7 WebView sign-in | Q10 | not started; a TV-style device-code probe was run on the Mac | |

Also built for the gates: a **gate app** (BBMusic's, in [samples/gate-app/](../samples/gate-app/), [`scripts/build-gate.sh`](../samples/gate-app/build-gate.sh)): a headless engine plus a one-page UI, to test what needs an installed `.bar` (headless launch, token handshake, app swiped away). Hyperberry's 0.3 app was the same idea.

## Samples

- [samples/engine-cxx20/protocol-example.md](../samples/engine-cxx20/protocol-example.md)
- [samples/engine-link/tcp/EngineLink.cpp](../samples/engine-link/tcp/EngineLink.cpp)
- [samples/descriptor/bar-descriptor-headless.xml](../samples/descriptor/bar-descriptor-headless.xml)
- [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp)
- [samples/engine-cxx20/server.cpp](../samples/engine-cxx20/server.cpp)
- [samples/engine-link/stdio/EngineLink.cpp](../samples/engine-link/stdio/EngineLink.cpp)
- [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml)
- [samples/engine-cxx20/main-child.cpp](../samples/engine-cxx20/main-child.cpp)
- [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt)
- [samples/engine-cxx20/gitignore](../samples/engine-cxx20/gitignore)
- [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)
- [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)

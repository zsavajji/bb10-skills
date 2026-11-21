# Testing and debugging: proving it works

Read this when: you are about to claim something works, add a test or a log line, read a phone log, or hand a build to the user.

Paths in parentheses are relative to the folder that holds the app repos side by side (the author's `bb10/`; adjust to where yours are). The evidence labels and the "builds, not works" rule are in [01-target-and-rules.md](01-target-and-rules.md) section 7; this doc is the method. Pre-build UI checks: [07-ui-build.md](07-ui-build.md). Phone access, test binaries, installs: [10-phone-access-and-install.md](10-phone-access-and-install.md).

## 1. The test pyramid these apps use

| Level | What | Where it runs | Source |
|---|---|---|---|
| 1. Engine tests | C++: host CMake build with ASan+UBSan, then `ctest`. Go: `go vet` and `go test` in the build image. Fuzz targets for parsers of network bytes | the Mac (or PC) | `phoenix/scripts/test-engine.sh`, `bbmusic/scripts/test-engine.sh`, `hyperberry/scripts/test-engine.sh`, `bbmusic/scripts/fuzz-engine.sh` |
| 2. UI checks before the build | `lupdate` parse, handler check, lint, node tests ([07-ui-build.md](07-ui-build.md) section 4) | the Mac, in the build script | `phoenix/scripts/build-app.sh` |
| 3. A gate or probe binary | pushed to `/root/<app>` and run from the root shell | the phone | `tengram/scripts/push.sh`, `bbmusic/tools/gates/`, `tengram/probe/` |
| 4. The installed app, logs read over SSH | install a `.bar`, open it, read `data/` logs and `slog2info` | the phone | `tengram/docs/HANDOFF.md` section 5 |
| 5. The user's eyes | nobody but the user can see the screen | the phone | `hyperberry/docs/memory/working-with-the-user.md` |

Do the cheapest level that can show the thing, in this order, and say which level a claim reached.

### Level 1 in detail

**C++ engine (phoenix, bbmusic).** `scripts/test-engine.sh`, verbatim shape:

```sh
B=$ROOT/build/engine-${1:-host}
"$ROOT/scripts/fetch-third-party.sh"
export PKG_CONFIG_PATH=${PKG_CONFIG_PATH:-}:/opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig
cmake -S "$ROOT/engine" -B "$B" -DCMAKE_BUILD_TYPE=Debug >/dev/null
cmake --build "$B" -j"$(getconf _NPROCESSORS_ONLN)"
if [ "${LIVE:-0}" = 1 ]; then
	ctest --test-dir "$B" --output-on-failure
else
	ctest --test-dir "$B" --output-on-failure -LE live
fi
```

The header says "Host build of the engine with ASan/UBSan, then ctest. `scripts/test-engine.sh x` builds in build/engine-x (default build/engine-host). `LIVE=1` also runs the tests that need the network." Tests labelled `live` need the network and are skipped by default. bbmusic can also run the host engine as a server: `build/engine-host/bbmusicd --data /tmp/x --null`, then talk to it with the token in `/tmp/x/engine.json`, to try a UI change that needs a new command (`bbmusic/docs/plan/ui.md`). Host and target builds are separate: [04-engine-cxx20.md](04-engine-cxx20.md).

**Go engine (hyperberry).** `hyperberry/scripts/test-engine.sh` builds the `hyperberry-build` image and runs, inside it with `GOWORK` set to the patched hypermeow work tree:

```sh
go vet ./... && go test ./...
```

"The engine is plain Go, so it also builds for Linux; only the `*_qnx.go` files and the phone itself need the BB10 build." A new request field must be tested through the socket, not by calling the handler directly: a field of the same name and another type failed the whole request as `bad_request`, unseen by tests that called the handler (`hyperberry/docs/plan/pitfalls.md`, 7c, 0.0.1.209). More: [05-engine-go.md](05-engine-go.md).

**Fuzzing** (`bbmusic/scripts/fuzz-engine.sh [seconds per target, 60]`): libFuzzer targets in `engine/tests/fuzz` for each parser of bytes from the network or a file: `webm_demux`, `webm_index`, `ump`, `pb`, `np_protocol`, `player_response`, `innertube_json`. Needs Homebrew's llvm ("Apple's clang has no libFuzzer"); builds with `-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined`; seeds each corpus from fixtures; exits 1 if any target crashed; everything lands in `build/fuzz/`.

**Tengram** has no host CMake build: its engine builds only for BB10. It tests with check programs built for the phone (`tests/voice_check.cpp`, `tests/https_check.cpp`, `tests/dbq.cpp`, built by `engine/build.sh`), probes (`probe/`), `tests/engine_smoke.py` and `tests/engine_client.py` against a test engine under `/root`, and a Mac harness (below) (`tengram/docs/HANDOFF.md` section 5).

## 2. Simulated back ends

Build and test the whole app without the real service:

- **hyperberry's simulated account** (`engine/sim.go`, `engine/sim_chats.go`): a file `data/sim` makes the engine simulate the account, so the whole app was built and tested without a real number. `scripts/sim.py` drives it from the Mac over an SSH tunnel; see [10-phone-access-and-install.md](10-phone-access-and-install.md) section 8. Chats are numbered as in `engine/sim_chats.go`: check the file before relying on a number. The sim writes its history only when it links. A notification check needs the app minimised.
- **tengram's Mac harness** (`build/harness/`, git-ignored, unreviewed): a scripted fake of TDLib's `ClientManager` with drives `drive2` to `drive13`. Built by sub-agents with a patched script that lived in a scratchpad; "bringing it into `tests/` is still worth doing" (`tengram/docs/HANDOFF.md` section 5).
- **bbmusic's `--null` sink and `--wav`** (`bbmusic/engine/src/main.cpp`): run the engine on the phone or the Mac with no speaker.
- **Tests on a real account** only in a safe place: tengram's stay in Saved Messages, and **never `link.logout`** a session the user needs (`tengram/docs/HANDOFF.md` section 3). Do not run `sim.py` on a phone that holds the real account.

Checks that need the engine and the UI together but not the real service: a protocol client (`tengram/tests/engine_client.py <cmd> '<json>' --listen 10`) through an SSH tunnel.

## 3. Logs

| App | Where (in the sandbox `data/` unless noted) | Notes |
|---|---|---|
| phoenix | `ui.log` | a file past 512 kB is removed and starts afresh (`phoenix/app/src/UiLog.h`) |
| bbmusic | `bbmusic-ui.log` (every line sent and received, except positions and the hello), `bbmusicd.log` (every command, state and track change) | no cap in `UiLog.h`; never logs the engine's token (`bbmusic/docs/plan/ui.md`) |
| hyperberry | `logs/ui.log`, `logs/hyperberryd.log`, `logs/crash.log`; also `slog2info -b hyperberry` | `ui.log` is rotated at start past 256 KB (one `.1` kept); `crash.log` keeps old crashes; QML errors to `hyperberry-qml-errors.log` in `/accounts/1000/shared/misc` or `data/` (`hyperberry/app/src/main.cpp`, `hyperberry/engine/main.go`) |
| tengram | `logs/ui.log`, `logs/tengramd.log`; `slog2info \| grep -i tengram` | engine log: the previous file kept once it passes 1 MB (`tengram/engine/main.cpp`) |
| older convention | `/accounts/1000/shared/misc/<app>.log`, capped (an earlier app: past 2 MB keep the newest 1 MB), copied by the user to the PC (into the project's `debug/`; once it landed in `build/`: look in both) | `base-bb10/skills/bb10-build-deploy/SKILL.md` |

Rules, each from an incident:

- **`qDebug` and `qWarning` go nowhere on the phone** unless routed: `main.cpp` sends them, QML errors included, to the system log and to the file. "The file outlives the process: the system recycles a closed app's log within minutes, and a freeze on 0.0.1.132 left nothing to read" (Q10, 2026-11-02). Flush every line (`fflush`): "a freeze or a kill keeps every line before it" (`hyperberry/app/src/main.cpp`).
- **A headless app's stderr goes nowhere**: the engine opens its log file first thing, before anything that can fail (`tengram/engine/main.cpp`).
- **The start line must carry** the version or build, the arguments it was given (and which it ignored), the data folder, and the memory limits: tengram's `start: service, data ..., ignored: [invoke://localhost]` and `rlimit DATA: soft ..., hard ...`; hyperberry's `UI started, pid N`, `ui: main N ms after launch`, `engine memory at start: ...`, `rlimit DATA: ...`; the skill: "one line per start giving the version/build, size, and arguments". The first failed allocation logs `out of memory: an allocation failed` with the limits ([09-headless-service.md](09-headless-service.md)).
- **Check a log's mtime and that it covers the build in question** before concluding anything. "A stale log looks like a fresh failure" (`hyperberry/docs/plan/pitfalls.md`). When a phone log disagrees with the code, first compare the build and arguments the log reports with what was built.
- **Cap log size** (table above), and never log secrets (the engine token).
- **Both sides log commands**, so a session can be reconstructed: bbmusic 0.1.0.4 neither logged commands and "the rest of the session could not be reconstructed" (`bbmusic/docs/plan/phase1.md`).
- **`crash.log` keeps old crashes, and an out-of-memory dump's first stack is who asked last, not who took the memory**: grep a dump for every frame of your own code and read `git log` around its date before blaming (`hyperberry/docs/plan/pitfalls.md`, 2026-11-20).
- **Where a QML error shows**: the UI log's `QML:` lines (phoenix, bbmusic) or `hyperberry-qml-errors.log`; with `data/check-qml` present, `check: ok/FAIL` lines for every document ([07-ui-build.md](07-ui-build.md) section 4.4).

## 4. Crash and failure signatures

| What you see | Likely cause | Go to |
|---|---|---|
| The app quits at start, nothing on screen, no log | a QML error in the root document, or a missing UI log line `QML:` | [07-ui-build.md](07-ui-build.md) section 4; lint and `lupdate` |
| A page never appears, or opens black | that page's document failed to load (unknown signal, duplicate function, `readonly property`, `Component.onCompleted`, a `bb.system` type with `onCreationCompleted`) | 07 section 4 and 6; the `data/check-qml` run names the file |
| `Invalid asset path` | a `cascades10` build (`.so`) with an executable descriptor | 07 section 2 |
| `Process N (name) terminated SIGSEGV code=1 fltno=11 ip=...` in a shell; "Process crashed" from QProcess | a native crash | the app's log; skill, "Logs and crashes" |
| `FATAL:... Check failed` and SIGTRAP | Chromium code | skill, "Logs and crashes" |
| The first `throw` crashes with SIGSEGV in `libz.so.2@_Unwind_GetLanguageSpecificData` | a toolchain image older than bb10-cxx20 v19.1.7-4 bound the unwinder to `libz` | [04-engine-cxx20.md](04-engine-cxx20.md) section 3: pin v19.1.7-4 or later, where the driver fixes it |
| SIGFPE right after OpenSSL init | CPU-feature probing left the thread without an FPU | [06-third-party-libs.md](06-third-party-libs.md) |
| Builds, then "symbol not found" on the phone only | an import missing from `libc.so.3` / `libm.so.2` | `base-bb10/skills/bb10-cxx20/SKILL.md`; the import check in [05-engine-go.md](05-engine-go.md) |
| The engine exits before `main` (47 ms), the UI's invoke gets error 6 `Target`, or `std::bad_alloc` / `out of memory` | the headless memory cap | [09-headless-service.md](09-headless-service.md) |
| The engine exits 60-80 ms after launch | it took the navigator's arguments for an error | 09 section 1 |
| The engine is slain about 6 s after launch | it did not connect to the navigator in time | 09 section 1 (`slog2info \| grep navigator`) |
| The engine never starts after START is refused, or the CPU is busy for hours | START from a background app is refused; a retry loop without back-off | 09 section 1 |
| Old behaviour after an install | the installer kept the old app: Package-Version unchanged | [08-packaging.md](08-packaging.md) section 3; `actual_version::` |
| "Operation not permitted" / exit 126 running a binary | not trusted by `pathtrust` | [10-phone-access-and-install.md](10-phone-access-and-install.md) section 3 |
| An install of an addon "does nothing" | the app kept its first-launch limit | 09: close all processes of the app once |
| Nothing happens though the code looks right | a permission missing from the descriptor | 08 section 5 |

BB10 can kill an app without a clean exit: flush files and settings as you go (skill; `hyperberry/docs/plan/pitfalls.md`, untested).

## 5. Measuring: method, not results

What the sources measured and how. Numbers are in the cited docs and belong to the phone and build named there.

- **Start-up time.** hyperberry logs `ui: main N ms after launch` and `ui: scene set` using the process start time from `/proc/self/as` (`DCMD_PROC_INFO`, `start_time`), so the system's share (loading libraries) is separated from the app's (`hyperberry/app/src/main.cpp`). Also the UI logs its start, the first list and a chat's first page, which with the Hub tap's line time a cold start and a tap (`hyperberry/docs/plan/phase8.md` 8.4). Tap to sound in bbmusic: the time from a `play` command in the UI log to the engine's next "state playing" (`bbmusic/scripts/measure-phone.py`).
- **CPU and memory.** `pidin -p <pid> mem` (data size) and `pidin -p <pid> times` (utime + stime), sampled; CPU share = seconds used between two snapshots over the time between them. `bbmusic/scripts/measure-phone.py` takes a snapshot (saved to `build/measure/<time>.json`) and, given an earlier one, the difference. It finds the app's processes by the package name in `pidin -F "%a %N" ar`. Its `DATA` path hard-codes the installed package's folder name (with its hash): update it after a reinstall under a new id. `tengram/probe/measure.sh` runs the probe detached and samples `pidin` into `tdprobe-mem.log`; RSS is read with `pidin -p <pid> mem`, not in the probe.
- **Idle battery.** A plugged-in phone charges, so an overnight number comes from an unplugged phone: the engine logs one line an hour with its memory, goroutines and battery level (`stats:` lines, hyperberry phase 8.4); `measure-phone.py` reads the charge from `/pps/system/BattMgr/status` (`StateOfCharge`, `ChargerStatus`) with `cat`; the planned sampler uses `AverageCurrent`. bbmusic's battery was "not measured: the phone stayed on USB (ssh runs over it)" (`bbmusic/docs/plan/ui.md` step 2.12).
- **Probe programs.** `tengram/probe/tdprobe.cpp`: "Measures TDLight on BB10: start time, memory, CPU, `getMemoryStatistics`"; each output line starts with `[ms since process start]`; `kill -USR1 <pid>` prints a status now (`tengram/probe/README.md`).
- **Layouts** are measured from the user's screenshots (the Q10 is 720 x 720, 1 du = 9 px), not guessed.

## 6. Handing a build to the user

1. Bump `<buildId>` (the build scripts do it through `-buildIdFile`).
2. QML checks pass: exit 0, warnings read ([07-ui-build.md](07-ui-build.md)).
3. Build and package.
4. Check `Package-Version` in `META-INF/MANIFEST.MF`, and grep one changed QML line inside the bar (`unzip -p build/app.bar native/assets/X.qml | grep ...`).
5. Give the path of the `.bar` and a short numbered list of what to try on the phone, naming which build each item needs.

Then:

- Say "builds", not "works", until the phone confirms. Say what the build contains: "0.1.0.2 had a placeholder engine and was taken for a working build" (`bbmusic/docs/plan/ui.md`).
- Say exactly what was verified and how, and name the path that ran: the wipe was first claimed from the sim's link, which runs the same code as Unlink; the user caught that Unlink itself had not been pressed (`hyperberry/docs/memory/working-with-the-user.md`, 2026-11-01).
- Say what is untested and on which model: code for a model you have no phone for can only be "builds; not tried on a <model>" (the author has a Q10 and a Classic, so that means the Passport and the on-screen keyboard phones) ([01-target-and-rules.md](01-target-and-rules.md) section 6).
- A build or probe that needs the user's finger is first checked as far as it can be without them (the engine can be driven on the phone from the Mac).
- Install only when it will not interrupt what the user is doing: an install closes the app and its engine, the user reopens it ([10-phone-access-and-install.md](10-phone-access-and-install.md)). Test lists must be doable as written: a check that needs two notes in one chat needs them set up first.
- The user "tests fast and reports in one line"; match each report to the logs before touching code. "Batch fixes, one build" when they ask (`bbmusic/docs/plan/ui.md`, "How the user tests").
- One commit per step with what was verified and what only compiles (`tengram/docs/HANDOFF.md` section 2). Screenshots are never committed.

## 7. Recording results

Gates and phone findings are written down with date, phone model and build number, as the `phase0.md` files do. Pattern, from `bbmusic/docs/plan/phase0.md`:

- A heading per gate with its verdict and where it ran: `## 0.3 A C++20 engine on the phone that reaches Google, first half passed, 2026-11-14 (Q10)`.
- What ran (file, how it was built and pushed), the output lines as a code block, "found on the way", what was not run ("Never run: the UI, the headless launch ...").
- A closing table: gate, result, with partial results spelled out ("passed with the app open ... and from a root shell with no Qt at all; not yet from the headless engine itself").
- For user rounds: the user's words quoted, then the logs read against them ("The gate app on the Q10, first round (0.0.1.3), run by the user").
- hyperberry's `phase0.md` records time stamps and sizes ("started 21:04, alive at 21:36, 6444K of data throughout") and keeps "[Q10]", "[SDK]", "[untested]" tags in `pitfalls.md`.
- Update `status.md` when a phase ends; docs for hyperberry live on the Mac only and are not in git (`hyperberry/docs/memory/docs-not-in-git.md`).
- Gate order and lessons: [02-architecture.md](02-architecture.md) section 8.

## Where sources disagree

- `tengram/docs/ui-fork.md` section 6 says "no `Component.onCompleted`, no `Connections` in Cascades' QML (connect in `onCreationCompleted`)"; `hyperberry/docs/plan/pitfalls.md` says "`QtObject`, `Timer` and `Connections` are fine" (with `import QtQuick 1.0`). Not settled by a source; treat `Connections` as unproven on the phone.
- The old convention of logging to `/accounts/1000/shared/misc/<app>.log` (skill) against logs in the sandbox `data/logs` (hyperberry, tengram, 2026-11): the sandbox is newer; only the former needs `access_shared`.

## Sources

- `phoenix/scripts/test-engine.sh`, `bbmusic/scripts/test-engine.sh`, `bbmusic/scripts/fuzz-engine.sh`, `hyperberry/scripts/test-engine.sh`, `bbmusic/scripts/measure-phone.py`, `hyperberry/scripts/sim.py`, `hyperberry/scripts/test-phone.js`
- `tengram/tests/` (listing), `tengram/probe/README.md`, `tengram/docs/HANDOFF.md` (sections 2-5), `tengram/docs/ui-fork.md` (section 6), `tengram/engine/main.cpp`, `tengram/engine/build.sh`
- `phoenix/app/src/UiLog.h`, `bbmusic/app/src/UiLog.h`, `hyperberry/app/src/main.cpp`, `bbmusic/app/src/main.cpp`
- `bbmusic/docs/plan/ui.md`, `bbmusic/docs/plan/phase0.md`, `bbmusic/docs/plan/phase1.md`
- `hyperberry/docs/plan/pitfalls.md`, `hyperberry/docs/plan/phase0.md`, `hyperberry/docs/plan/phase8.md`, `hyperberry/docs/memory/working-with-the-user.md`, `hyperberry/docs/memory/docs-not-in-git.md`
- `base-bb10/skills/bb10-build-deploy/SKILL.md`, `base-bb10/skills/bb10-cxx20/SKILL.md`

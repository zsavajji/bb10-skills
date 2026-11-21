# engine-cxx20

A C++20 engine for BB10: CMake, entry points, server, scripts. Copied from the app repos; not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| CMakeLists.txt | phoenix, `engine/CMakeLists.txt` (4e06969) | verbatim | Engine build with the bb10-cxx20 toolchain |
| main-child.cpp | phoenix, `engine/src/main.cpp` (4e06969) | verbatim | Engine run as a child process of the UI |
| main-headless.cpp | bbmusic, `engine/src/main.cpp` (1d80813) | verbatim | Engine run as the headless service |
| server.h, server.cpp | bbmusic, `engine/src/server/` (1d80813) | verbatim | Loopback server, token, engine.json |
| navigator.h, navigator.cpp | bbmusic, `engine/src/platform/` (1d80813) | verbatim | Calling the Navigator (invoke) from the engine |
| main-tdlib-service.cpp | tengram, `engine/main.cpp` (5a65cdf) | excerpt | Args, log file, rlimit logging, flock, set_new_handler; app logic cut and marked |
| check.h | phoenix, `engine/tests/check.h` (4e06969) | verbatim | Minimal test macros |
| protocol-example.md | bbmusic, `docs/protocol.md` (1d80813) | verbatim | A UI/engine protocol written down |
| scripts/build-engine.sh, test-engine.sh, fetch-third-party.sh | phoenix, `scripts/` (4e06969) | verbatim | Docker cross-build, host tests, pinned downloads |
| scripts/fuzz-engine.sh | bbmusic, `scripts/` (1d80813) | verbatim | Fuzzing the parsers |
| scripts/link-tdlib.sh | tengram, `engine/build.sh` (5a65cdf) | verbatim | Linking an engine against TDLib |
| gitignore | phoenix, `.gitignore` (4e06969) | verbatim | What to ignore |

The scripts expect their own app's layout (`$ROOT/engine`, `$ROOT/scripts`).
| bb10.cpp | Tengram, `engine/bb10.cpp` (5a65cdf) | verbatim | Navigator and network status on a BPS thread of their own |
| hub_qnx.cpp | Tengram, `engine/hub_qnx.cpp` (5a65cdf) | verbatim | BlackBerry Hub backend loaded at run time (Unified Data Source) |
| engine.h, engine.cpp | BBMusic, `engine/src/engine/engine.h, engine.cpp` (1d80813) | verbatim | Engine wiring: threads, commands to player, events to clients |
| CMakeLists-bbmusic.txt | BBMusic, `engine/CMakeLists.txt` (1d80813) | verbatim | Engine CMake with wolfSSL, curl and Opus |
| checks/dbq.cpp | Tengram, `tests/dbq.cpp` (5a65cdf) | verbatim | Run one SQL statement on the encrypted chats.db from a shell |
| checks/https_check.cpp | Tengram, `tests/https_check.cpp` (5a65cdf) | verbatim | One TLS request to see the answer on the phone |
| checks/voice_check.cpp | Tengram, `tests/voice_check.cpp` (5a65cdf) | verbatim | Opus/OGG voice codec round trip |
| probe/build.sh | Tengram, `probe/build.sh` (5a65cdf) | verbatim | Build the TDLib probe in Docker |
| probe/measure.sh | Tengram, `probe/measure.sh` (5a65cdf) | verbatim | Sample memory and CPU of a process on the phone |
| probe/README.md | Tengram, `probe/README.md` (5a65cdf) | verbatim | What the probe measures and how to run it |
| probe/tdprobe.cpp | Tengram, `probe/tdprobe.cpp` (5a65cdf) | verbatim | TDLib start time, memory and CPU measurement |

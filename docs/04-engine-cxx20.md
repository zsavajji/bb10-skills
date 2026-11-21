# Building a C++20 engine

Read this when: you build the engine process of a new app in C++20 with bb10-cxx20: CMake layout, cross build, host tests, link order, the `-lz` trap and its fix, and the limits of the toolchain.

Template: **phoenix** (newest). Variations: **bbmusic** (same shape, audio, fuzz targets, the phone's SQLite) and **tengram** (hand-written link line for TDLib). Why a separate process, and the repo layout: [02-architecture.md](02-architecture.md). Compilers and images: [03-toolchains.md](03-toolchains.md). Libraries (OpenSSL, curl, wolfSSL, opus, webp): [06-third-party-libs.md](06-third-party-libs.md). Headless packaging: [09-headless-service.md](09-headless-service.md).

## 1. `engine/CMakeLists.txt` (from [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt))

Header and the BB10 switch, verbatim:

```cmake
cmake_minimum_required(VERSION 3.20)
project(phoenix LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CROSSCOMPILING OR QNX)
  set(PHX_BB10 ON)
else()
  set(PHX_BB10 OFF)
endif()

option(PHX_SANITIZE "ASan + UBSan on the host build" ON)
add_compile_options(-Wall -Wextra)
if(PHX_SANITIZE AND NOT PHX_BB10)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
```

Rules this encodes:

- **One switch, `<PFX>_BB10`**, set from `CMAKE_CROSSCOMPILING OR QNX`. Everything that differs between the phone and the Mac hangs off it: where libraries come from, what links, whether tests exist. (bbmusic: `BBM_BB10`.)
- **The same sources build twice**: for the phone with `-DCMAKE_TOOLCHAIN_FILE=/opt/bb10-cxx20/share/bb10-cxx20.cmake`, and on the Mac (host compiler, Homebrew libs) with **ASan + UBSan on by default**.
- Phone-only code lives in a module whose sources are empty on the host: bbmusic's `platform` module (navigator, now-playing, Bluetooth routing) is `#ifdef __QNX__` (BBMusic's engine CMakeLists: "BB10 only: its sources are empty on the host").

### One static library per module, with a dependency table

```cmake
set(PHX_MODULES util net text mp4 reddit media setup core)
set(deps_util)
set(deps_net util)
set(deps_text util)
set(deps_mp4)
set(deps_reddit net util text)
set(deps_media net util mp4)
set(deps_setup util net)
set(deps_core util net text mp4 reddit media setup)

foreach(m ${PHX_MODULES})
  file(GLOB srcs CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${m}/*.cpp)
  ...
  if(srcs)
    add_library(${m} STATIC ${srcs})
    set(vis PUBLIC)
  else()
    add_library(${m} INTERFACE)  # no sources yet: headers only
    set(vis INTERFACE)
  endif()
  target_include_directories(${m} ${vis} ${CMAKE_CURRENT_SOURCE_DIR}/src)
  foreach(d ${deps_${m}})
    target_link_libraries(${m} ${vis} ${d})
  endforeach()
  ...
endforeach()
```

(`...` marks lines of the source file left out here: the `util` module adds yyjson's `yyjson.c` compiled with `-w`, and `net`/`media` take their third-party targets.) Add a module by listing it and its `deps_<name>`; sources are globbed from `src/<module>/*.cpp`. Includes are written `"<module>/header.h"` relative to `src/`.

| bbmusic module | depends on |
|---|---|
| util, proto, platform | (none; platform: util) |
| net | util |
| stream | proto net util |
| media, audio, store | util |
| innertube | net util |
| player | stream media audio |
| server | util net proto stream media audio innertube player store platform |
| engine (the wiring) | all of the above |

(BBMusic's engine CMakeLists.) The executable is one `add_executable(<name>d src/main.cpp)` linked to the top module (`core` in phoenix, `engine` in bbmusic), with the version from `project(... VERSION)` passed as a define (`PHX_VERSION`, `BBM_VERSION`).

### Tests: one program per file

```cmake
if(NOT PHX_BB10)
  enable_testing()
  foreach(m ${PHX_MODULES})
    add_custom_target(${m}_tests)
    file(GLOB tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/${m}/*_test.cpp)
    foreach(t ${tests})
      get_filename_component(file ${t} NAME_WE)       # json_test, http_live_test
      string(REGEX REPLACE "_test$" "" name ${file})  # json, http_live
      set(exe ${m}_${name}_test)
      add_executable(${exe} ${t})
      ...
      add_test(NAME ${m}.${name} COMMAND ${exe})
      if(file MATCHES "_live_test$")
        set_tests_properties(${m}.${name} PROPERTIES LABELS live)
      endif()
```

- Tests are `tests/<module>/*_test.cpp`, **one program each**, linked to their module; a hand-rolled `tests/check.h` is the only helper.
- A file named `*_live_test.cpp` gets the ctest label `live` and needs the network; `ctest -LE live` skips it.
- Fixtures live in `tests/fixtures`, passed as `PHX_FIXTURES` / `BBM_FIXTURES`. Tests are not built for the phone.

## 2. Build scripts, verbatim

### Cross build: [samples/engine-cxx20/scripts/build-engine.sh](../samples/engine-cxx20/scripts/build-engine.sh)

```sh
#!/bin/sh
# Cross-builds the engine for BB10 in the phoenix-build image into build/engine-bb10, then fails
# if the binary imports a libgcc unwinder (the -lz trap, phase0.md gate 0.3).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
"$ROOT/scripts/fetch-third-party.sh"
docker run --rm -v "$ROOT:/src" -w /src phoenix-build sh -euc '
	cmake -S engine -B build/engine-bb10 -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE=/opt/bb10-cxx20/share/bb10-cxx20.cmake >/dev/null
	cmake --build build/engine-bb10 -j"$(nproc)"
	unwinders=$(readelf --dyn-syms -W build/engine-bb10/phoenixd | grep -ci " UND .*unwind" || true)
	if [ "$unwinders" != 0 ]; then
		echo "build-engine.sh: phoenixd imports $unwinders unwinder symbols" >&2
		exit 1
	fi
	ls -l build/engine-bb10/phoenixd'
```

bbmusic's is identical with `bbmusic-build`, `build/engine-bb10/bbmusicd` (BBMusic's build-engine script). The unwinder check in it dates from before the toolchain's fix and a new app can leave it out (section 3). Note the image (`phoenix-build`) must exist: `docker build -t phoenix-build docker` (comment in [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl)).

### Host tests: [samples/engine-cxx20/scripts/test-engine.sh](../samples/engine-cxx20/scripts/test-engine.sh)

```sh
#!/bin/sh
# Host build of the engine with ASan/UBSan, then ctest. `scripts/test-engine.sh x` builds in
# build/engine-x (default build/engine-host). LIVE=1 also runs the tests that need the network.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
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

Runs on the Mac, not in Docker (needs Homebrew curl, opus and so on). `scripts/test-engine.sh x` builds in a second folder, so two agents can build at once.

### Fuzz targets: [samples/engine-cxx20/scripts/fuzz-engine.sh](../samples/engine-cxx20/scripts/fuzz-engine.sh) [seconds per target, default 60]

For every parser of network bytes (WebM demuxer, UMP, protobuf, now-playing text protocol, player-response JSON, InnerTube JSON), one libFuzzer target `engine/tests/fuzz/<name>_fuzz.cpp` built with `-fsanitize=fuzzer,address,undefined`. It needs Homebrew's `llvm` (`/opt/homebrew/opt/llvm/bin/clang++`; Apple's clang has no libFuzzer), seeds a corpus from fixtures, runs each target for the time given, and exits 1 if any crashed. Rationale: C++ parsers of network bytes have no memory safety, so ASan/UBSan tests plus fuzz targets replace it (BBMusic's plan §3).

## 3. The `-lz` trap, and its fix in the toolchain

**What happened.** The phone's `libz.so.2` exports a libgcc unwinder (`_Unwind_*`), and so do 18 other SDK shared libraries (19 in all, libc and libm among them: [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README). The older driver put the program's libraries ahead of the C++ runtime on the link line, so with `-lz` the linker bound the runtime's unwinder calls to `libz`, and **the first `throw` crashed**: SIGSEGV in `libz.so.2@_Unwind_GetLanguageSpecificData` (BBMusic's phase 0 notes gate 0.3, Q10, 2026-11-14, with the `bb10-cxx20:latest` of that day).

**It is fixed in the toolchain, and you rely on that fix** (the owner's decision, 2026-11-21). Since bb10-cxx20 **v19.1.7-4** the driver links `libunwind.a` whole, before the program's objects and libraries, so no SDK library can supply the unwinder: "Linking `libz.a` by path is no longer necessary" ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s release notes, [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README "The unwinder ahead of the program's libraries"). For a new app:

- Pin a toolchain release of v19.1.7-4 or later ([03-toolchains.md](03-toolchains.md) section 1). Then `-lz`, or `libz.so` by path as Tengram links it, is fine, and so is any other SDK library.
- You do not need the `libz.a` workaround or the `readelf` check below.

**What the apps do, and why they differ:**

| App | Links zlib as | Why |
|---|---|---|
| Tengram | `/opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.so`, after `--end-group` ([samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh), [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh)), no check | relies on the driver's fix; its engine runs on the Q10 and the Classic |
| Phoenix, BBMusic | `/opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.a` by full path, and `build-engine.sh` fails the build when the binary imports an unwinder | the workaround from BBMusic's gate 0.3, written before the fix; it still works and was never removed |

The workaround's check, for reading those two scripts or for a build on an image older than v19.1.7-4:

```sh
readelf --dyn-syms -W <binary> | grep -ci " UND .*unwind"    # must print 0
```

Still open in the toolchain's own notes: its new tests `exceptions-z` and `exceptions-z-shared` "have not been run on a phone yet" (as of the v19.1.7-5 notes).

## 4. Link order and system libraries

- **Static libraries first, in dependency order, then `-lsocket`.** Phoenix: `libcurl.a`, `libssl.a`, `libcrypto.a`, then `libz.a`, then `-lsocket` (comment in [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt): "Order matters: curl, ssl, crypto, then libz.a and -lsocket"). bbmusic: `libcurl.a`, `libwolfssl.a`, `libopus.a`, then `libz.a` and `-lsocket` (kept in a `bbm_syslibs` interface target linked last).
- **`CURL_STATICLIB`** must be defined for code that includes `<curl/curl.h>` when libcurl is static (`target_compile_definitions(... CURL_STATICLIB)`).
- **`-lbps` is not added by this driver** (XIAZY's `bb10-cc` adds it; `bb10-clang++` does not). Add `-lbps`, `-lscreen`, `-lEGL -lGLESv2` yourself if used ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README Limitations; [skills/bb10-cxx20](../skills/bb10-cxx20/SKILL.md)).
- System libraries the engines link, as used:

| Library | Used for | Where |
|---|---|---|
| `-lsocket` | sockets (curl, the server) | all |
| `-lm` | math | tengram |
| `-lbps` | navigator, BPS events, notifications | bbmusic `platform`; tengram |
| `-lasound -laudio_manager` | speaker through libasound opened via `audio_manager`; Bluetooth routing (`-laudio_manager`) | bbmusic `audio`, `platform` |
| `-lmmrndclient -lstrm` | tengram engine's system libs | [samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh) |
| `-lsqlite3` | the **phone's own SQLite** (no unwinder in it, has WAL): bbmusic `store` links it rather than an amalgamation | BBMusic's engine CMakeLists |

- The phone's SQLite version differs between notes (3.8.7.1 measured by HyperBerry, 3.7.14.1 stated for bbmusic): write SQL that both accept. bbmusic: "no SQL newer than that, so no upsert" (BBMusic's plan §8). BBMusic's plan §13 says the SQLite amalgamation is "fetched, pinned" but the CMake file links `-lsqlite3`; the CMake file is newer and wins.
- TLS libs and curl flags: [06-third-party-libs.md](06-third-party-libs.md).
- Hand `curl` the CA bundle at run time: phoenix and bbmusic call `http_set_ca_file("app/native/assets/cacert.pem")` under `#ifdef __QNX__` if the file is readable (`CURLOPT_CAINFO`; env override `PHX_CA_FILE` / `BBM_CA_FILE` for the host).

## 5. Tengram's hand-written link line ([samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh))

TDLib is a pile of static libraries with circular references, so tengram does not use CMake for the engine; it runs one `bb10-clang++` in the `tengram-build` image. The pieces, verbatim:

```sh
INC="-isystem build/prefix/include -isystem third_party/td/tddb -isystem third_party/td/tdutils \
-isystem build/td-bb10/tdutils -isystem third_party/td -isystem third_party/td/sqlite \
-isystem calls -isystem third_party/tgcalls -isystem third_party/tgcalls/tgcalls -DTGCALLS_USE_STD_OPTIONAL"
LIBS="-Wl,--gc-sections -Wl,--start-group $L/libtdclient.a $L/libtdcore.a $L/libtdapi.a $L/libtdmtproto.a \
$L/libtdnet.a $L/libtddb.a $L/libtdsqlite.a $L/libtdactor.a $L/libtde2e.a $L/libtdutils.a \
$L/libtgcalls.a $L/libtg_owt.a $L/libssl.a $L/libcrypto.a $L/libopus.a -Wl,--end-group \
/opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.so -lsocket -lm"
```

- **`-Wl,--start-group ... --end-group`** around the static archives resolves their circular references. **`-Wl,--gc-sections`** drops unused code; the OpenSSL and TDLib objects are built with `-ffunction-sections -fdata-sections` for it.
- **`-isystem`** keeps TDLib's header warnings out of the output.
- **ThinLTO**: `${TD_LTO:+-flto=thin}` is added to the compile/link when the libraries were built with LTO (`TD_LTO` defaults to on; `TD_LTO=` empty if built with `TD_LTO=OFF`). Archives then hold LLVM bitcode; code is generated at link time. This needs a bb10-cxx20 with the LTO plugin (the `emutls-lto` image).
- **Separate strip**: `bb10-clang++ ... -o build/tengramd.unstripped $LIBS -lbps -lmmrndclient -lstrm`, then `/opt/bb10-toolchain/bin/arm-unknown-nto-qnx6.5.0eabi-strip -o build/tengramd build/tengramd.unstripped`. Keep the unstripped file for symbolizing crashes. (37 MB static stripped: Tengram's handoff notes §4.)
- **Keys compiled in from the environment, never written to a file** (the packaged service gets no environment of ours): `engine/build.sh` passes `-e TG_API_ID -e TG_API_HASH` into the container and adds `-DTENGRAM_API_ID=$TG_API_ID -DTENGRAM_API_HASH="$TG_API_HASH"` only when both are set. `scripts/build-app.sh` reads them from the environment or from a git-ignored key file at the repo root. Never put keys in a tracked file, notes or memory (Tengram's handoff notes §3).
- Extra tools built the same way: `build/dbq` (database query tool for the phone, which has no `sqlite3`) and `build/voice_check`.

## 6. Toolchain limits to design around ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README)

| Limit | Consequence |
|---|---|
| **"C" locale only**: `std::locale("")` and named locales throw (no `newlocale` in BB10 libc) | treat text as UTF-8 bytes; do locale-dependent work yourself |
| **`thread_local` is emulated** (`-femulated-tls`): a call to `__emutls_get_address` per access (33 ns measured on a phone with v19.1.7-5, OS 10.3.3, 2026-11-16) | keep `thread_local` out of hot loops |
| **No `tzdb`** in `std::chrono`; **no floating-point `from_chars`** (libc++ 19) | convert time zones and parse floats yourself |
| `<filesystem>`: file times have **1-second resolution**; `remove_all` works by path (no `openat`) | do not rely on sub-second mtimes |
| `std::jthread`, `stop_token`, `osyncstream` are on (`_LIBCPP_ENABLE_EXPERIMENTAL`) | usable |
| libc gaps filled by `libbb10-cxx20-rt.a`, linked automatically: `aligned_alloc`, `asprintf`/`vasprintf`, `getline`/`getdelim`, `memmem`, `memrchr`, `strchrnul`, `mkostemp`, `pipe2`, `dup3`, `madvise`, `malloc_usable_size`, `wcsnlen`, `mbsnrtowcs`, `wcsnrtombs` | anything else missing is a link error or an import that fails only on the phone (section 7) |
| **ARM only** | no x86 simulator runtimes |

**Static vs shared C++ runtime.**

- **Static, hermetic (the default)**: libc++/libc++abi/libunwind are linked in with hidden symbols, one private copy per executable or `.so`. Needed because BB10's libc/libm export a libgcc `_Unwind_*` that system libraries in the same process use. Do not let C++ objects or exceptions cross between two modules that each carry their own copy. All four engines use it.
- **`-shared-libc++`**: for a program made of several `.so` that share C++ types, RTTI or exceptions. Ship **`libc++_shared.so.1`** (1.5 MB) in the `.bar` **under `native/lib`**, which is on the app's `LD_LIBRARY_PATH` via `<env var="LD_LIBRARY_PATH" value="app/native/lib:/usr/lib/qt4/lib"/>`. Ship shared libraries under their SONAME; symlinks are not carried ([skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md)). With `-shared-libc++`, `libc++_shared.so.1` must be the first library needed ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s test build script checks it).
- Libraries of other languages' ABIs: a C API behind its own `.so` (`libtgs.so`, [03-toolchains.md](03-toolchains.md)).
- Frame pointers are dropped when optimising (v19.1.7-4); `-fno-omit-frame-pointer` still wins (host ASan build uses it).
- A C++ program that uses neither exceptions nor libc++ is about 36 KB larger, since the unwinder is always linked in (v19.1.7-4 notes).
- LTO: `-flto` and `-flto=thin` work (v19.1.7-5): `bb10-clang++ -O2 -flto -c engine.cpp` then `bb10-clang++ -O2 -flto engine.o -o engine`; CMake `CMAKE_INTERPROCEDURAL_OPTIMIZATION` and `check_ipo_supported()` work with the toolchain file. LTO tests are "builds, not tried on a phone" in the README.

## 7. "Builds here, fails on the phone": missing imports

A symbol missing from the phone's `libc.so.3` / `libm.so.2` links fine against the SDK headers and fails only at load on the phone ("symbol not found"). Check every import against the libraries the phone has.

- **bb10-cxx20's own check** ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s test build script): collect the defined dynamic symbols of the phone's libraries (`nm -D --defined-only`), then for each binary `nm -D --undefined-only | awk '$1 == "U" { print $2 }' | sort -u | comm -23 - "$defined"`; anything left is "not on BB10". Fix by avoiding the function, or by adding it to bb10-cxx20's `src-rt/` and `include/bb10-compat/`.
- **HyperBerry's version** ([samples/engine-go/build-engine.sh](../samples/engine-go/build-engine.sh)), for the libs its Go engine uses:

```sh
L=$QNX_TARGET/armle-v7/lib
for l in $L/libc.so.3 $L/libm.so.2 $L/libsocket.so.3 $L/../usr/lib/libbps.so.3 \
		$L/../usr/lib/libunifieddatasourcec.so.1 $L/libmmrndclient.so.1 $L/../usr/lib/libstrm.so.1; do
	nm -D --defined-only $l | awk "{ print \$3 }"
done | sed "s/@.*//" | sort -u >/tmp/defined
missing=$(nm -D --undefined-only /src/build/hyperberryd | awk "\$1 == \"U\" { print \$2 }" |
	sed "s/@.*//" | sort -u | comm -23 - /tmp/defined)
[ -z "$missing" ] || { echo "not on BB10: $missing"; exit 1; }
```

  The C++ Phoenix and BBMusic scripts have no such check; add this block when your engine links more system libraries.
- A build that links is not a build that loads: "Builds, then 'symbol not found' on the phone" is the first thing to check ([skills/bb10-cxx20](../skills/bb10-cxx20/SKILL.md) "When something breaks"). Also: exceptions that crash instead of being caught point at the `.ARM.exidx` link script (`share/bb10-exidx.ld` is added to every link; keep it if you write a custom link line) and at `R_ARM_TARGET2` (fixed in libc++abi).
- **Trap for the phone-side run:** a copied binary runs only after `/proc/boot/pathtrust !<path>` on a file never executed before; a refused file stays refused, even a new file on the same inode. Always use the `!`; without it the whole filesystem is trusted. BBMusic's push-phone script and [samples/phone/push-engine.sh](../samples/phone/push-engine.sh) do it: a fresh name per build (`<name>-<sha8>-<n>`), a refused copy parked in `denied/`. Name data files so they do not match `<program>-*` (the push script removes older copies by that pattern). Details: [10-phone-access-and-install.md](10-phone-access-and-install.md).

## 8. Engine conventions seen in the apps

Read from the three `main.cpp` files; confirm in them before copying.

| Convention | Where |
|---|---|
| **Arguments**: `--data DIR`, `--version`; **ignore the navigator's own arguments** instead of exiting (bbmusic and tengram once exited in 60-80 ms on them). Phoenix and bbmusic print usage for an unknown `--x` and ignore plain words, which is how the navigator's own arguments (package name, `invoke://localhost`) pass | [samples/engine-cxx20/main-child.cpp](../samples/engine-cxx20/main-child.cpp), [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp), [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp) |
| Default data dir is **`$HOME`** (the sandbox's `data/`), else `data` | phoenix, bbmusic |
| `--data` for a **test run outside the sandbox**; without it the engine is the packaged service (tengram) | Tengram's handoff notes §3 |
| `signal(SIGPIPE, SIG_IGN)`; `SIGINT`/`SIGTERM` **blocked before any other thread exists**, then one `sigwait` thread asks the engine to stop cleanly (removes `engine.json`) | phoenix, bbmusic |
| **Log to a file in the data folder** (`phoenixd.log`, `bbmusicd.log`, `logs/tengramd.log`, previous one kept once it passes 1 MB): a headless app's stderr goes nowhere | the three `main.cpp` |
| **Log a start line**: `"<name> <version>, data in <dir>"`; tengram also logs **its rlimits** at start (`rlimit DATA: soft ..., hard ...`) and `out of memory: an allocation failed` on a failed allocation; that line says whether the phone has the memory patch | [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp), Tengram's headless-limit notes |
| **Navigator first** (headless): bbmusic starts `Navigator().start(request_exit)` right after blocking signals, "it slays a headless process that has not reached it within 6 s"; the navigator's exit request becomes a clean stop | [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp) |
| **Exit by itself after idle**: bbmusic exits after **ten minutes with nothing playing and no UI connected**; it does not start at boot (`PlayState::Stopped` or `Paused` with 0 authenticated clients) | BBMusic's plan §3, BBMusic's engine code (`idle_check`) |
| Tengram: no timer of its own; one `poll()` loop; **one engine per data folder** via an `flock` on `engine.lock` (exit code 3 if taken); `umask(077)` | [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp) |
| Phoenix: exits when stdin closes | Phoenix's protocol notes |
| `engine.json` written and removed as described in [02-architecture.md](02-architecture.md) §3 | |
| Fuzz targets for parsers of network bytes; every test runs under ASan/UBSan on the Mac | [samples/engine-cxx20/scripts/fuzz-engine.sh](../samples/engine-cxx20/scripts/fuzz-engine.sh), BBMusic's plan §3 |
| Keep the sample path in integers (no libm call per sample): `std::lrint` per sample cost 18.6 % of a core vs 5.7 % with an integer multiply and shift | BBMusic's phase 0 notes gate 0.4 |

## Samples

- [samples/engine-cxx20/CMakeLists.txt](../samples/engine-cxx20/CMakeLists.txt)
- [samples/engine-cxx20/main-child.cpp](../samples/engine-cxx20/main-child.cpp)
- [samples/engine-cxx20/scripts/build-engine.sh](../samples/engine-cxx20/scripts/build-engine.sh)
- [samples/engine-cxx20/scripts/test-engine.sh](../samples/engine-cxx20/scripts/test-engine.sh)
- [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl)
- [samples/engine-cxx20/main-headless.cpp](../samples/engine-cxx20/main-headless.cpp)
- [samples/engine-cxx20/scripts/fuzz-engine.sh](../samples/engine-cxx20/scripts/fuzz-engine.sh)
- [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus)
- [samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh)
- [samples/engine-cxx20/main-tdlib-service.cpp](../samples/engine-cxx20/main-tdlib-service.cpp)
- [samples/engine-cxx20/server.cpp](../samples/engine-cxx20/server.cpp)
- [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh)
- [samples/engine-go/build-engine.sh](../samples/engine-go/build-engine.sh)
- [samples/phone/push-engine.sh](../samples/phone/push-engine.sh)

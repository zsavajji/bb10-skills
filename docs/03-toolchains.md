# Toolchains: images and which compiler builds what

Read this when: you pick a compiler for a new piece of code (UI, engine, ported library), set up the Docker images, or hit a build trap that the sources already recorded.

Everything builds from the Mac in Docker (OrbStack). There is no SDK on the Mac itself: the editor's red C++ errors for `bb/cascades/...` includes are noise (Tengram's handoff notes §4; HyperBerry's pitfalls notes). Context: [01-target-and-rules.md](01-target-and-rules.md), [02-architecture.md](02-architecture.md).

## 1. The two base images

| | `yamsergey/bb10-ndk:0.6` | `bb10-cxx20` |
|---|---|---|
| What | BlackBerry Native SDK **10.3.1.995** | clang 19 + LLVM libc++/libc++abi/libunwind **19.1.7**, on XIAZY's `blackberry10-toolchain` image |
| Has | `qcc`, `qmake` (Qt 4.8 + Cascades), `lupdate`, `lrelease`, `blackberry-nativepackager`, `blackberry-deploy` | `bb10-clang`, `bb10-clang++`, armnto binutils, the SDK sysroot, CMake toolchain file |
| Builds | the **Cascades UI** and small C shims | **engines**, ported libraries, CLI tools |
| Platform | linux/386 only: on Apple Silicon it runs emulated, and Docker warns "platform (linux/386) does not match the detected host platform" (seen running it, 2026-11-21) | x86-64 and arm64 ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README: "on an Apple Silicon Mac they run natively") |
| Env | `source /root/bbndk/bbndk-env_10_3_1_995.sh` | `/opt/bb10-cxx20/bin` on `PATH` |

Sources: [skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md), [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh), [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README.

### NDK image paths ([skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md))

- target: `/root/bbndk/target_10_3_1_995/qnx6` (`usr/include`, `armle-v7/usr/lib`, `armle-v7/usr/lib/qt4/imports` with the QML `plugins.qmltypes`)
- host tools: `/root/bbndk/host_10_3_1_12/linux/x86/usr/bin`
- device profiles: `/root/bbndk/target_10_3_1_995/blackberry-sdk-descriptor.xml`

### bb10-cxx20 image paths (from the Dockerfiles and scripts)

- toolchain file: `/opt/bb10-cxx20/share/bb10-cxx20.cmake`
- SDK sysroot: `/opt/bb10-sdk/target/qnx6` (libs in `armle-v7/usr/lib`, headers in `usr/include`)
- binutils: `/opt/bb10-toolchain/bin/arm-unknown-nto-qnx6.5.0eabi-{ar,ranlib,nm,strip,as,readelf}`

### Getting bb10-cxx20 ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README, `Dockerfile.release`)

```sh
docker build -t bb10-cxx20 .                                  # from the bb10-cxx20 repo's Dockerfile: builds LLVM runtimes, about 3 min
docker build -f Dockerfile.release -t bb10-cxx20:19.1.7-6 .   # same image from a release tarball (checked against its sha256), no build
docker pull ghcr.io/zsavajji/bb10-cxx20:latest                # published image (also :v19.1.7-6)
```

`Dockerfile` and `Dockerfile.release` both start from XIAZY's image pinned by digest (`ghcr.io/xiazy/blackberry10-toolchain@sha256:60ce49510a37ed92368cca010bbaabf97b12d209b9fab179e8491b266bb2578f`).

### Which local tag each app uses

| App | Base tag | Where |
|---|---|---|
| bbmusic, phoenix | `bb10-cxx20:latest` | [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus), [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl) |
| hyperberry | `bb10-cxx20:19.1.7-6-build` | [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go) |
| tengram | `bb10-cxx20:emutls-lto` (build arg `BASE`) | [samples/docker/Dockerfile.tdlib](../samples/docker/Dockerfile.tdlib), [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh) |

**A new app should pin a release tag, not `latest`.** HyperBerry's Dockerfile comment on its move to `19.1.7-5` said why: it had "frame pointers dropped when optimising, the unwinder and mbsnrtowcs fixes, faster emulated TLS, the LTO plugin; 'latest' was still the 2026-10-31 image (the user's ask, 2026-11-17)". `latest` is whatever was last built locally, so a rebuild on another Mac can silently differ. What each release fixed ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s release notes):

| Release | Change that matters to you |
|---|---|
| (v19.1.7-3 and before) | exceptions crashed on the phone until the `R_ARM_TARGET2` fix |
| v19.1.7-4 | driver links `libunwind.a` first, so `-lz` no longer hijacks `throw`; `@file` response files; `mbsnrtowcs`; no frame pointers when optimising |
| v19.1.7-5 | `-flto` / `-flto=thin`, CMake IPO; faster `thread_local`; `thread_local` destructors at thread exit |
| v19.1.7-6 | **NEON by default** (`-mfpu=neon`; on a Q10 integer loops 2.6-10x faster, float loops stay scalar without `-ffast-math`); `temp_directory_path()` falls back to `/var/tmp` |

`tengram`'s `emutls-lto` is "bb10-cxx20 with the QNX emulated-TLS runtime and LTO" ([samples/docker/Dockerfile.tdlib](../samples/docker/Dockerfile.tdlib)); Tengram's draft LTO patch is the draft that became the LTO support. The unwinder trap and its fix are in [04-engine-cxx20.md](04-engine-cxx20.md) section 3.

## 2. The per-app build images

All four layer on bb10-cxx20; each pins its downloads by sha256 (`ADD --checksum=`). Library recipes: [06-third-party-libs.md](06-third-party-libs.md).

- **`phoenix-build`** ([samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl), `FROM bb10-cxx20:latest`): adds `perl patch make`, then **OpenSSL 3.5.7** (static, ARM assembly, with the required `armcap` patch) and **libcurl 8.23.0** (OpenSSL backend), both installed in `/opt/phoenix`.
- **`bbmusic-build`** ([samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus), `FROM bb10-cxx20:latest`): **wolfSSL 5.9.4**, **libcurl 8.23.0** (wolfSSL backend) and **libopus 1.6.1**, installed in `/opt/bbmusic`.
- **`tengram-build`** ([samples/docker/Dockerfile.tdlib](../samples/docker/Dockerfile.tdlib), `ARG BASE=bb10-cxx20:emutls-lto`): adds `gperf perl patch libssl-dev zlib1g-dev` (the last two for the **host** pass of TDLib's `prepare_cross_compiling`). OpenSSL, libopus and TDLight are built into `build/prefix` by `scripts/build-deps.sh`, not baked into the image.
- **`hyperberry-build`** ([samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go), `FROM bb10-cxx20:19.1.7-6-build`): copies `/usr/local/go` from the **zsavajji/go-qnx 1.27.4** image (pinned by digest), sets `PATH` and `GOTOOLCHAIN=local`, builds **libopus 1.6.1 twice** (BB10 sysroot, and the host for PC tests), adds `patch` for the hypermeow patches. See [05-engine-go.md](05-engine-go.md).

The UI of every app is built in `yamsergey/bb10-ndk:0.6`, not in these images. (Tengram also runs `qcc -Vgcc_ntoarmv7le` there to build its `tg-install-patcher` helper and `tgs_check`: Tengram's build-app script, [samples/third-party/build-tgs.sh](../samples/third-party/build-tgs.sh).)

## 3. THE COMPILER QUESTION

The NDK image has two GCCs and no others. `qcc -V` in `yamsergey/bb10-ndk:0.6` (run 2026-11-21) lists **4.6.3 and 4.8.3**, each with the targets `gcc_ntoarmv7le`, `gcc_ntoarmv7le_cpp`, `gcc_ntoarmv7le_cpp-ne` and `gcc_ntoarmv7le_gpp` (and the same four for `ntox86`). **There is no 4.8.4.** Both versions have a folder under `$QNX_TARGET/armle-v7/lib/gcc/`. To see it yourself:

```sh
docker run --rm yamsergey/bb10-ndk:0.6 bash -c 'source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null; qcc -V'
```

The variant names: `_cpp` is Dinkumware's C++ library (what Qt and Cascades use), `_cpp-ne` the same without exceptions, `_gpp` GNU libstdc++.

### 3.1 Cascades/Qt UI code: two options in use

**Default: `qcc -Vgcc_ntoarmv7le`** = GCC 4.6.3 with **Dinkumware's C++ library** (`libcpp.so.4`) and partial C++11 (no `override`, no `final`; the older note in HyperBerry's pitfalls notes says "GCC 4.6.3, so C++98: no `auto`, lambdas, `nullptr` or range-for; `SIGNAL`/`SLOT`"). BBMusic's [`.pro`](../samples/ui/bbmusic.pro) and [samples/ui/app.pro](../samples/ui/app.pro) set no compiler, so they use this default.

**GCC 4.8.3 with `-std=gnu++11`** (user, 2026-11-20): HyperBerry's [`.pro`](../samples/ui/hyperberry.pro) and [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro). The qmake lines, copied exactly from [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro):

```
QMAKE_CC = qcc -V4.8.3,gcc_ntoarmv7le
QMAKE_CXX = qcc -V4.8.3,gcc_ntoarmv7le_cpp
QMAKE_LINK = $$QMAKE_CXX
QMAKE_LINK_SHLIB = $$QMAKE_CXX
QMAKE_LINK_C = $$QMAKE_CC
QMAKE_LINK_C_SHLIB = $$QMAKE_CC
QMAKE_CXXFLAGS += -std=gnu++11
LIBS += -L$$(QNX_TARGET)/armle-v7/lib/gcc/4.6.3 -lcpp
```

HyperBerry's [`.pro`](../samples/ui/hyperberry.pro) has the same lines. Both also have `QMAKE_CXXFLAGS += -Wno-psabi` and `QMAKE_LFLAGS += -Wl,-rpath-link,$${QNX_DIR}/$${QNX_CPUDIR}/usr/lib/qt4/lib`.

Why, from the comments in those two files:

1. The spec's `qcc -Vgcc_ntoarmv7le` takes the NDK's default, GCC 4.6.3, whose C++11 is partial (no `override` or `final`).
2. 4.8.3 ships in the same NDK, but with **GNU libstdc++ only** (`lib/gcc/4.8.3`), while Qt and Cascades are built against **Dinkumware** (`libcpp.so.4`).
3. So C++ is **compiled by 4.8.3's `_cpp` variant** (Dinkumware's headers) and **linked against 4.6.3's Dinkumware library** (the `libcpp.so.4` that Qt and Cascades load). Result: **one standard library in the process**, as before.
4. **`gnu++11`, not `c++11`**: strict ISO hides the POSIX calls the app makes (`open`, `getpid`, `clock_gettime`).
5. **Not C++20 in the UI**: clang's libc++ would sit beside Dinkumware in the same process. The engine and `libtgs.so` are clang's; `libtgs.so` shows the UI a C API only.

GCC 4.8.3 builds Cascades apps: these two apps use it. Evidence: **verified on the phone, Q10 and Classic, 2026-11-20.** The plans and handoffs don't mention it; the commits do. Tengram's commit `f154d37` ("UI on gcc 4.8.3 ... Built, installed in 0.1.0.64 to 0.1.0.83 on the Q10 and the Classic") made the switch, and later builds ran on both phones (1.0.0.93 on the Q10, 1.0.0.94 to 1.0.0.102 on the Classic, commit `ad5ae3d`). HyperBerry followed in commit `acdf8a1`, and its later builds ran on the Q10 (0.3.1.382, commit `852f471`).

If you copy this setup into a new app, change nothing in the six `QMAKE_*` lines and keep the `-lcpp` path.

**Clean the build folder once after changing the compiler.** A `.pro` change doesn't make the objects stale: HyperBerry's 0.3.1.378 still had 4.6.3's objects until `build/ui` was cleaned (commit `acdf8a1`).

Which to pick for a new app: 4.8.3 with `gnu++11` if you want `override`, `final` and the rest of C++11 in UI code; the 4.6.3 default (written as C++98, as BBMusic and Phoenix are) if the UI is thin. Both are proven on the phone. BBMusic and Phoenix were not moved, and no source says why.

### 3.2 C in the UI

Use **`-std=gnu99`, not `c99`**: strict ISO C hides the POSIX functions (`open`, `fcntl`, `pthread_mutexattr_settype`) that SQLite's amalgamation calls, so it does not compile ([samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro): `QMAKE_CFLAGS += -std=gnu99`). HyperBerry's [`.pro`](../samples/ui/hyperberry.pro) uses `-std=c99` and builds only because its one C file, qrcodegen, calls no POSIX function. C++ is separate: GCC 4.8.3 does C++11 with `-std=gnu++11` (section 3.1). HyperBerry's [`.pro`](../samples/ui/hyperberry.pro) uses `QMAKE_CFLAGS += -std=c99` (its only C file there is the qrcodegen QR encoder plus libwebp); tengram's is the newer and wider rule.

C-only defines for qcc go through `-Wc,"-D..."`: `DEFINES` reach C++ only ([skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md)). Plain C third-party code can also be compiled by the UI's `.pro`: libwebp (decoder only), qrcodegen, SQLite ([06-third-party-libs.md](06-third-party-libs.md)).

### 3.3 Engines and ported libraries

Use `bb10-clang` / `bb10-clang++` (C++20, `-std=gnu++20` by default) from bb10-cxx20: `std::thread`, real exceptions, `<format>`, ranges, `<filesystem>`. See [04-engine-cxx20.md](04-engine-cxx20.md).

**Never link libc++ code into the Cascades process.** If the UI needs a C++14+ library, build it with clang into its own `.so` that exports a **C API only**, and call it as plain C. The one case: Tengram's **`libtgs.so`** (rlottie, needs C++14), built by [samples/third-party/build-tgs.sh](../samples/third-party/build-tgs.sh) with libc++ linked in statically and a version script (`tests/tgs.map`), shipped in the bar's `native/lib`; the UI links `-L$$PWD/../build/prefix/lib -ltgs -lz` ([samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)). Never pass `std::string`, containers or exceptions across ([bb10-cxx20](https://github.com/zsavajji/bb10-cxx20)'s README "Not for Cascades/Qt code").

C libraries (OpenSSL, zlib, mongoose) can also be built with the NDK's GCC 4.6 and linked statically into the UI ([skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md): `CC=ntoarmv7-gcc AR=ntoarmv7-ar RANLIB=ntoarmv7-ranlib ./Configure gcc no-shared no-tests no-asm no-ui-console`). Tengram instead links a clang-built `libcrypto.a` into the UI for SQLCipher: it then needs the `__aeabi_mem*` helpers and `-lsocket` ([06-third-party-libs.md](06-third-party-libs.md)).

### 3.4 Decision table

| Code kind | Compiler | Image | Why |
|---|---|---|---|
| QML-backed C++ (models, controllers, `EngineLink`), anything linking Qt 4.8 or `bb::*` | `qcc -Vgcc_ntoarmv7le` (default) **or** `qcc -V4.8.3,gcc_ntoarmv7le_cpp` + `-std=gnu++11` | `yamsergey/bb10-ndk:0.6` | Qt and Cascades use Dinkumware's `std::`; the second option adds `override`/`final` and fuller C++11 while keeping one library |
| C inside the UI (SQLite amalgamation, libwebp, qrcodegen) | `qcc` with `-std=gnu99` | same | strict ISO hides POSIX calls |
| Engine, anything reaching the OS only through C APIs | `bb10-clang++` (C++20) | `bb10-cxx20:<tag>` or an app image on it | libc++ 19, exceptions that unwind, `std::thread` |
| Library needing C++14+ that the UI must call | `bb10-clang++` into its own `.so` with a C API only | the app's build image | one standard library per module; no `std::` across the boundary |
| C library for the engine | `bb10-clang` | same | matches the engine's runtime |
| C library for the UI | NDK GCC (`ntoarmv7-gcc`) or clang with care (aeabi helpers) | NDK image / app image | see 3.3 |
| Go engine | go-qnx 1.27.4; C parts by `bb10-clang` | `hyperberry-build` | [05-engine-go.md](05-engine-go.md) |
| Tiny helper started by the app (Tengram's `tg-install-patcher`) | `qcc -Vgcc_ntoarmv7le -O2` | NDK image | Tengram's build-app script |

## 4. Docker traps recorded in the sources

| Trap | What to do | Source |
|---|---|---|
| Docker is **rootless** here: files written as root in the container belong to the user on the host | **Never `chown` in the container**: it maps to uid 100999 and locks the user out | [skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md) |
| **zsh does not word-split** `$vars` | wrap loops over file lists in `sh -c` / `bash -c` | same |
| The mount covers **only the project** | files from sibling repos must be **copied in** first (Tengram copies the [rlimit-patch](https://github.com/zsavajji/authman-rlimit) build's `headless-mem.zip` to `build/headless-mem.zip`; a symlink would not resolve in the container) | Tengram's build-app script |
| **Single-quoted `bash -c '...'` blocks cannot contain an apostrophe** | no apostrophes in comments or strings inside; Tengram's build.sh escapes one as `'"'"'` | [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh) ("No apostrophes in here: this is one single-quoted string of the outer shell"), [samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh) |
| The Mac **sleeps** and kills long builds | start `caffeinate -i -m -s -t <s>` before long stretches | Tengram's handoff notes §4 |
| **OrbStack can hang after sleep** | `orbctl stop`, wait 100 s, `orbctl start` | same |
| Docker may read a **just-edited file at its old size** (a Go file failed to parse at its old length; gofmt saw a file cut short) | run it again; after a build, check the `.bar` holds the change: `unzip -p build/<app>.bar native/assets/X.qml \| grep ...` | same; HyperBerry's pitfalls notes |
| Chained build steps with `;` hide an early failure | chain with `&&` | HyperBerry's pitfalls notes (0.8) |
| Two builds at once clash in one build folder | give each its own (`build/ui-a`, `build/ui-b`); files two agents add in parallel go into the `.pro` by a glob (`$$files($$PWD/src/*Bubble.cpp)`: relative to `$$PWD`, since qmake runs in the build folder; re-run qmake after adding one) | HyperBerry's pitfalls notes (8.6) |
| `td_api` units take over 1 GB of RAM each | `JOBS=2 scripts/build-deps.sh` if the compiler is OOM-killed | [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh) |
| "Nothing to be done" from `make` after a QML-only change | fine: QML ships as assets | [skills/bb10-build-deploy](../skills/bb10-build-deploy/SKILL.md) |
| After adding a library to `LIBS`, qmake may not regenerate | `grep -c lbps build/Makefile` | same |
| The packager writes `assets/.assets.index` into the source tree | delete it after packaging (`rm -f assets/.assets.index`) and git-ignore it | [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh); the other apps' `.gitignore` |
| [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20) is the user's repo | do not change it from an app | Tengram's handoff notes §4 |

Where to run `qmake`/`make` and package: [07-ui-build.md](07-ui-build.md), [08-packaging.md](08-packaging.md). Rebuild the runtimes themselves with `scripts/fetch-llvm.py` then `scripts/build-runtimes.sh SRC PREFIX` inside XIAZY's image ([skills/bb10-cxx20](../skills/bb10-cxx20/SKILL.md)); for a new app you almost never need to.

## Samples

- [samples/docker/Dockerfile.openssl-curl](../samples/docker/Dockerfile.openssl-curl)
- [samples/docker/Dockerfile.wolfssl-curl-opus](../samples/docker/Dockerfile.wolfssl-curl-opus)
- [samples/docker/Dockerfile.tdlib](../samples/docker/Dockerfile.tdlib)
- [samples/docker/Dockerfile.go](../samples/docker/Dockerfile.go)
- [samples/ui/app.pro](../samples/ui/app.pro)
- [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)
- [samples/third-party/build-deps.sh](../samples/third-party/build-deps.sh)
- [samples/third-party/build-tgs.sh](../samples/third-party/build-tgs.sh)
- [samples/engine-cxx20/scripts/link-tdlib.sh](../samples/engine-cxx20/scripts/link-tdlib.sh)
- [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh)

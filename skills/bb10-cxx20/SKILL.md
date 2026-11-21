---
name: bb10-cxx20
description: Build C and C++20 programs and libraries for BlackBerry 10 (QNX 6.6, ARMv7 softfp) with the bb10-cxx20 toolchain — clang 19 plus LLVM libc++/libc++abi/libunwind 19.1.7 on XIAZY's blackberry10-toolchain Docker image (pin the release tag 19.1.7-5). Use when compiling native non-Cascades code for BB10 with modern C++ (std::thread, <format>, ranges, <filesystem>, exceptions, LTO), porting a Linux/POSIX library or a browser/JS engine to BB10, cross-building with CMake or autotools for BB10, choosing static vs shared libc++, linking zlib (the -lz unwinder trap), exposing a C++14+ library to the Cascades UI through a C-API-only .so, running a test binary on the phone (pathtrust), or debugging link errors, missing libc symbols, or exception crashes on a BB10 phone. Not for Cascades/Qt 4.8 UI code, which stays on the SDK's qcc (see bb10-build-deploy).
---

# bb10-cxx20: modern C++ for BlackBerry 10

Image: **pin a release tag, not `latest`**: `bb10-cxx20:19.1.7-5`, built from the release with
`docker build -f Dockerfile.release -t bb10-cxx20:19.1.7-5 .` (a Dockerfile that starts from XIAZY's image, pinned by digest, and
unpacks the release tarball, checked by sha256), or the published `ghcr.io/zsavajji/bb10-cxx20:v19.1.7-5` (also `:latest`; x86-64 and
arm64, so it runs natively on Apple Silicon). `latest` is whatever was last built locally, so a rebuild on another Mac can silently differ.
Where each app got its base tag, and the NDK image: [03](../../docs/03-toolchains.md) section 1. The engine build in full:
[04](../../docs/04-engine-cxx20.md). Everything in [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20) is the user's repo: do not change it from an app.

| Release | What changed for you (`out/release*/notes.md`) |
|---|---|
| v19.1.7-3 and before | every `throw` crashed on the phone until the `R_ARM_TARGET2` fix |
| **v19.1.7-4** | the driver links `libunwind.a` whole, before the program's objects and libraries, so `-lz` no longer hijacks `throw`; `@file` response files expanded by the driver (CMake's Ninja); `mbsnrtowcs` counts a cut character's bytes as glibc does; **no frame pointers when optimising** (`-fno-omit-frame-pointer` still wins; about 2% less text); a program using neither exceptions nor libc++ is about 36 KB larger (the unwinder is always linked in) |
| **v19.1.7-5** | **LTO**: `-flto` and `-flto=thin`, CMake `CMAKE_INTERPROCEDURAL_OPTIMIZATION` and `check_ipo_supported()`, archives of bitcode objects indexed by the armnto `ar`; **faster `thread_local`**: `src-rt/emutls.c` reads the thread's array from `_cpupage_ptr->tls` with no call or barrier, **33 ns per access against 66 ns** with compiler-rt's, measured on a phone (OS 10.3.3, 2026-11-16, `tlsbench`); **`thread_local` destructors now run** at thread exit (before, a thread's variables were freed first) |

## Which compiler for which code

| Code | Compiler | Why |
|---|---|---|
| Cascades UI, anything linking Qt 4.8 / `bb::*` C++ APIs | NDK `qcc` in `yamsergey/bb10-ndk:0.6`: **GCC 4.6.3 default** (C++98) | Qt and Cascades on the phone use GCC's C++ ABI and Dinkumware `std::`; libc++ types and exceptions are not compatible |
| the same UI code that wants C++11 (`override`, `final`...) | **`qcc -V4.8.3,gcc_ntoarmv7le_cpp` + `-std=gnu++11`**, linked against 4.6.3's `libcpp` (HyperBerry, Tengram; verified on the Q10 and the Classic, 2026-11-20): still Dinkumware, one library in the process. The qmake lines are in bb10-build-deploy | [03](../../docs/03-toolchains.md) section 3.1 |
| a library that needs C++14+ and that the **UI must call** (rlottie in Tengram) | `bb10-clang++` into its **own `.so` that exports a C API only**, libc++ linked in statically, a version script (`tests/tgs.map`); shipped in the bar's `native/lib`; the UI links `-L... -ltgs` and calls it as plain C. Never pass `std::string`, containers or exceptions across | Tengram's `libtgs.so` (`scripts/build-tgs.sh`); [03](../../docs/03-toolchains.md) section 3.3 |
| Engines, ported libraries, CLI tools, anything that talks to the OS through **C** APIs (libc, Screen, EGL/GLES2, BPS, sockets) | `bb10-clang++` (this toolchain) | C++11-20 library, real `std::thread`, exceptions that unwind |
| Plain C | either; `bb10-clang` matches this toolchain's runtime | |
| C inside the UI (SQLite amalgamation, libwebp) | `qcc` with `-std=gnu99` | strict ISO hides the POSIX calls |

Other routes to modern C++ on BB10, for comparison: extrowerk's bb10-toolchain (GCC forks: 5.4 on its `700_release` branch, 8.3 on
`bb10-gcc` `master`, 9.3 with didacticode's patches), which an earlier plan chose for TDLib's C++17 before this toolchain existed.
bb10-cxx20 already gives C++20 with exceptions, threads and `thread_local`: use it for TDLib-class libraries unless a GCC-only feature
forces the other route. C libraries (OpenSSL, zlib, mongoose) also build with the NDK's own GCC and link straight into a Cascades app.

Connect the two halves with a **process boundary** (the engine, a plain child of the UI or a headless service; see
[02](../../docs/02-architecture.md)) or a **C interface** (`extern "C"`, POD structs, no exceptions crossing). Never pass `std::string`,
containers or exceptions between a qcc module and a libc++ module.

## Using it

```sh
# one-off, in the current directory
docker run --rm -v "$PWD:/src" -w /src bb10-cxx20:19.1.7-5 bb10-clang++ -O2 app.cpp -o app

bb10-clang++ -O2 -c engine.cpp                    # -std=gnu++20 unless -std= is given
bb10-clang++ -O2 engine.o -o engine               # static, hermetic C++ runtime (default)
bb10-clang++ -O2 -shared -o libfoo.so foo.cpp     # shared libraries work too
bb10-clang++ -O2 -shared-libc++ main.cpp -o app   # link libc++_shared.so.1 instead
bb10-clang++ -O2 -flto -c engine.cpp && bb10-clang++ -O2 -flto engine.o -o engine   # LTO (-flto=thin: ThinLTO, one job per CPU)
```

- CMake: `-DCMAKE_TOOLCHAIN_FILE=/opt/bb10-cxx20/share/bb10-cxx20.cmake` (find-root is the SDK sysroot plus the toolchain prefix;
  programs are never searched there).
- Autotools: `CC=bb10-clang CXX=bb10-clang++ --host=arm-unknown-nto-qnx6.5.0eabi` **and** `ac_cv_prog_cxx_cxx11=` (empty). Autoconf's
  C++11 probe assigns `u8""` to `const char*`, which fails under C++20's `char8_t`, and it then appends `-std=gnu++11`.
- `BB10_CC_VERBOSE=1` prints the clang and ld lines. `QNX_TARGET` points at another SDK target. `BB10_LTO_PLUGIN` names another linker plugin for `-flto`.
- **BPS**: XIAZY's `bb10-cc` adds `-lbps`; this driver does not. Add `-lbps` (and `-lscreen`, `-lEGL -lGLESv2`) yourself.
- ARM only: no x86 simulator runtimes.
- LTO: objects are LLVM bitcode, the driver loads `LLVMgold.so` into the armnto `ld` on every link; the images put the plugin where `ar`,
  `ranlib` and `nm` look (`/opt/bb10-toolchain/lib/bfd-plugins`); with the bare tarball do that `ln -sf` once. Tengram's `emutls-lto`
  tag is "bb10-cxx20 with the QNX emulated-TLS runtime and LTO" and builds TDLib with ThinLTO. LTO tests are **builds, not tried on a phone** in the notes.

## The `-lz` trap: fixed in the driver, rely on it

The phone's `libz.so.2` and 18 other SDK shared libraries (libscreen, liblzma; libc and libm too) export a libgcc unwinder (`_Unwind_*`).
The older driver named the program's libraries **ahead of the C++ runtime**, so with `-lz` one of them supplied the unwinder instead of
`libunwind.a` and **the first `throw` crashed**: SIGSEGV in `libz.so.2@_Unwind_GetLanguageSpecificData`. [Q10] (BBMusic gate 0.3,
2026-11-14, `bb10-cxx20:latest` of that day)

**Fixed in the driver in v19.1.7-4** (it links `libunwind.a` whole, first; "linking `libz.a` by path is no longer necessary"), and
**the fix is what you rely on** (the owner, 2026-11-21): pin v19.1.7-4 or later and link zlib as `-lz` or `libz.so`, like any other SDK
library. Tengram does (`libz.so` after `--end-group`); its engine runs on the Q10 and the Classic.

Phoenix and BBMusic still carry the workaround from before the fix: zlib by its archive and full path
(`/opt/bb10-sdk/target/qnx6/armle-v7/usr/lib/libz.a`) and a build that fails unless
`readelf --dyn-syms -W <binary> | grep -ci " UND .*unwind"` prints 0. It still works; a new app needs neither. The toolchain's own
tests `exceptions-z` and `exceptions-z-shared` had not been run on a phone as of the v19.1.7-5 notes.
[04](../../docs/04-engine-cxx20.md) section 3.

## Static or shared runtime

- **Static (default)**: libc++/libc++abi/libunwind linked in with hidden symbols, one private copy per executable or `.so`. Needed because BB10's
  libc/libm export a libgcc `_Unwind_*` that system libraries in the same process use. Do not let C++ objects or exceptions cross between two
  modules that each carry their own copy. All four apps' engines use it.
- **`-shared-libc++`**: for a program made of several `.so` that share C++ types, RTTI or exceptions. Ship `lib/libc++_shared.so.1` (1.5 MB)
  in the .bar under `native/lib` (`app/native/lib` is on the app's `LD_LIBRARY_PATH` per the descriptor `<env>`); it must be the first library needed.

## Limits to design around (README, v19.1.7-5)

- **"C" locale only**: `std::locale("")` and named locales throw (no `newlocale` in BB10 libc). UTF-8 as bytes is fine.
- `thread_local` is emulated (`-femulated-tls`): a short call per access, **33 ns** on a phone. Keep it out of hot loops.
- No `tzdb`, no floating-point `from_chars` (libc++ 19.1.7; the README's limitations line says "libc++ 20"). `<filesystem>` times have 1 s
  resolution; `remove_all` works by path (no `openat`).
- `std::jthread`, `stop_token`, `osyncstream` are on (`_LIBCPP_ENABLE_EXPERIMENTAL`).
- QNX 6.6 libc gaps are filled by `libbb10-cxx20-rt.a` (linked automatically): `aligned_alloc`, `asprintf`/`vasprintf`, `getline`/`getdelim`,
  `memmem`, `memrchr`, `strchrnul`, `mkostemp`, `pipe2`, `dup3`, `madvise`, `malloc_usable_size`, `wcsnlen`, `mbsnrtowcs`, `wcsnrtombs`, emulated
  TLS and atomic libcalls, `__dso_handle`. Anything else missing is a link error or, worse, an import that only fails on the phone.
- ARM only; `-lbps` not added (above).

## When something breaks

1. **Builds, then "symbol not found" on the phone**: the import is missing from BB10's `libc.so.3`/`libm.so.2`. `tests/build.sh` checks every
   import of a binary against the SDK's libraries (`nm -D --undefined-only` against the libs' defined symbols; the recipe is in
   [04](../../docs/04-engine-cxx20.md) section 7). Fix by adding the function to `src-rt/` (and its declaration to `include/bb10-compat/`) or by avoiding it.
2. **An exception crashes instead of being caught**: causes found and fixed: unwind tables outside `PT_ARM_EXIDX` (armnto's linker script lacked
   `.ARM.exidx` rules; `share/bb10-exidx.ld` is added to every link), libc++abi reading `R_ARM_TARGET2` GOT-relative (patched PC-relative for QNX), and
   the unwinder bound to an SDK library (the `-lz` trap above, on an image older than v19.1.7-4). If it recurs with a custom link line, keep the exidx script and run `tests/build.sh`'s exidx check.
3. **SDK header conflicts** (`cos`/`log` macros, `std::` C functions from Dinkumware, `<link.h>`, `<langinfo.h>`): the wrappers in
   `include/bb10-compat/` handle them; extend them rather than patching the SDK. A program's own `-I`/`-isystem` dirs come before them, so a
   bundled newer library (ICU, zlib) wins over the SDK's copy.
4. **`thread_local` destructors do not run** or `thread_local` is slow: you are on a release before v19.1.7-5.
5. Rebuilding the runtimes: `scripts/fetch-llvm.py` then `scripts/build-runtimes.sh SRC PREFIX` inside XIAZY's image (about 3 minutes; the
   Dockerfile does exactly this). A new app almost never needs it.

## Running it on the phone

Each of these changes the phone's state outside our scratch folder, except the first: ask before anything else ([10](../../docs/10-phone-access-and-install.md) section 1).

- **By hand, over root SSH (the main route).** `ssh bb10rootx` is a root shell over USB; it reaches the Q10 or the Classic (check `modelname`).
  Copy the program into the app's scratch folder (`/root/<app>`) and trust that one file: `/proc/boot/pathtrust !<file>` (always the `!`: without it the
  whole filesystem is trusted). A file it refused stays refused, even a new file reusing its inode, so every build gets a fresh name
  (`<name>-<sha8>-<n>`), goes through `/dev/shmem`, and a refused copy is emptied and parked in `denied/`. `tengram/scripts/push.sh` is the generic
  script (verbatim in [10](../../docs/10-phone-access-and-install.md) section 3). Run it with `</dev/null`, in the background when it serves.
  The phone's shell has no `head`, `tr`, `uniq`, `printf`: filter on the Mac.
- **A program started over ssh cannot load a shared library of ours**: `mmap` fails with EPERM for a `.so` outside the system's trusted file system,
  even after `pathtrust`; the same library loads from an **installed bar's `native/lib`**. So link a test program static, or test the library inside
  an installed app (Tengram's `libtgs.so`, commit `f154d37`). Root cannot execute an installed app's binaries either: test an installed build through the navigator.
- **Inside an app**: put the binary in the .bar as `<asset path="..." type="Qnx/Elf">` so it gets the execute bit; start it from the Cascades app
  with `QProcess` (`QCoreApplication::applicationDirPath()` is `app/native`), or as a headless entry point ([09](../../docs/09-headless-service.md)).
- **No SSH (fallback)**: copy to `/accounts/1000/shared/misc/...`, then from a terminal (term51) copy it into the terminal's home and `chmod +x`:
  shared storage may not keep the execute permission. The test suite runs this way: `sh run-tests.sh`; each test ends `all passed` or `FAILED`.
- **Engine habits** the apps share ([04](../../docs/04-engine-cxx20.md) section 8): `--data DIR`, default `$HOME`; **ignore the navigator's arguments**
  (package name, `invoke://localhost`) instead of exiting; log to a file in the data folder first thing (a headless stderr goes nowhere) with a start
  line giving version, data folder and `RLIMIT_DATA`; block `SIGINT`/`SIGTERM` before any thread and stop cleanly from a `sigwait` thread; a headless
  engine connects to the navigator first (slain after 6 s). **Never run the service binary bare from a root shell** (pass `--data`).
- **Memory**: a headless process is capped at 3 MiB without a memory class, 30 MiB with `_sys_headless_xlarge`, 128 MiB once the `rlimit-patch` addon
  runs ([09](../../docs/09-headless-service.md)). A 32-bit address space of about 2 GB is shared with graphics and network (one engine port kept V8's heap at 512 MB or less).
- Status of the toolchain's own tests: every test up to v19.1.7-3 passed on a phone (MSM8960, OS 10.3.3) from a .bar; `tlsbench` passed on 2026-11-16.
  The LTO tests, `exceptions-z*` and `cmake-test` as built now are **builds, not tried on a phone**.
- Third-party libraries (OpenSSL, curl, wolfSSL, opus, libwebp, TDLib, SQLCipher): [06](../../docs/06-third-party-libs.md).

# BlackBerry 10 app development: context for an agent

Paste this file into an agent's prompt before it works on a BB10 app. It is the short form of this repo
(`base-bb10`); each section names the doc with the detail and the samples. Everything here comes from four
working apps beside this repo: [hyperberry](https://github.com/zsavajji/hyperberry) (WhatsApp), [tengram](https://github.com/zsavajji/tengram) (Telegram), [bbmusic](https://github.com/zsavajji/bbytmusic) (YouTube Music) and
[phoenix](https://github.com/zsavajji/phoenix) (Reddit). This repo is self-contained: the code is in `samples/` (see the last section), and no app repo is needed.

## 1. What never changes

- **BB10 10.3.3 is the last version, forever** (the test phones: 10.3.3.3216). Never write OS version checks,
  fallbacks or "in case a future OS changes this" code. What works on the test phone works everywhere.
- **Every phone is rooted.** Root-only paths, PPS objects and root addons are product design. Never build
  "without root" fallbacks.
- **Nothing is signed.** The signing and debug-token servers died in January 2022. Never spend time on
  keystores, author certificates or debug tokens.
- **The stack:** QNX 6.6 (it reports itself as 8.0.0, so test for features), 32-bit ARMv7 softfp, a libc from
  2014, Qt 4.8.6, Cascades (`bb.cascades 1.4`). Not Qt 5. Every CPU has NEON and QNX has no runtime CPU
  detection: libraries get `-mfpu=neon` and their detection switched off at configure time.
- **The test phones** are a Q10 and a Classic. `ssh bb10rootx` reaches whichever is on the USB cable: run
  `grep modelname /pps/services/deviceproperties` before naming it.

Detail: `docs/01-target-and-rules.md`.

## 2. Rules for working on these apps

- Say **"builds"**, not "works", until the phone confirms. State what was verified, on which phone and build.
- **The phone is read-only** except a scratch folder of your own (`/root/<app>`). **Ask before every install**
  of a `.bar` or an addon and before any change outside that folder; approval does not carry over. Never
  restart a system process (restarting `authman` broke every app launch until a reboot). Ask before a reboot.
- **Every build handed over gets a new build id.** The installer keeps the old app when the version is the same.
- **Never remove a workaround the phone proved** because it looks redundant. Say so and let a phone test decide.
- Secrets (API keys) come from the environment or a git-ignored key file and are compiled in with `-D`. Never
  put them in a tracked file or in notes. Fetched third-party sources are pinned by sha256 and not committed.
- New apps are translated into: English (the source), it, fr, de, pl, ru, hi, vi, id.

## 3. The shape of an app: one `.bar`, two processes

```
 ┌─────────────────────────┐   NDJSON    ┌───────────────────────────┐
 │ UI: Cascades, Qt 4.8    │◄───────────►│ engine: C++20 or Go,      │◄──► the network
 │ C++/QML, built by qcc   │             │ no Qt, built by clang     │
 └─────────────────────────┘             └───────────────────────────┘
```

- **Why two processes:** Qt and Cascades use GCC's ABI with Dinkumware's C++ library; the engine uses libc++.
  They cannot share `std::` types or exceptions. The split also lets the engine outlive the UI and be tested
  on the Mac.
- **The engine runs one of two ways.**
  - *A plain child of the UI* (Phoenix): started with `QProcess`, NDJSON on its stdin and stdout, dies with
    the UI. Choose this when nothing has to happen while the app is closed. Before `QProcess::start`, set
    `FD_CLOEXEC` on every descriptor from 3 up, or the child inherits Cascades' connections.
  - *A headless service* (HyperBerry, Tengram, BBMusic): a second entry point started by an invoke; NDJSON
    over loopback TCP. The engine binds `127.0.0.1` port 0, makes a random 128-bit token, and writes
    `data/engine.json` (`{"port":…,"token":"…"}`, mode 0600). The UI reads it, connects, and its first line is
    `hello` with the token. Loopback TCP and not a Unix socket: a process killed while binding a named Unix
    socket crashes the phone's network stack until a reboot.
- **Framing:** one JSON object per line. Requests carry `"@"` (an id, a string) and `"cmd"`; replies echo the
  id with `"ok"`; events carry `"ev"`. Ids are strings everywhere. Write the contract in `docs/protocol.md`.
- **Engine language:** C++20 unless the protocol library you need is Go (HyperBerry's is). Go costs a 15 MB
  binary, a garbage-collected heap under the headless memory cap, and cgo for every native API.
- **Repo layout** (Phoenix's): `app/` (`src/`, `assets/`, `translations/`, `bar-descriptor.xml`, `<name>.pro`),
  `engine/` (`CMakeLists.txt`, `src/<module>/`, `tests/<module>/*_test.cpp`, `third_party/`),
  `docker/Dockerfile`, `scripts/`, `docs/protocol.md`, `build/` (git-ignored).
- **Gates first:** prove each risky part on the phone with the smallest program that can fail, before app code.

Detail: `docs/02-architecture.md`.

## 4. Toolchains: everything builds in Docker, on a Mac, Linux or WSL host

| Code | Compiler | Image |
|---|---|---|
| Cascades UI, anything linking Qt or `bb::*` | NDK `qcc`: GCC 4.6.3 (default, write C++98), or GCC 4.8.3 with `-std=gnu++11` | [`yamsergey/bb10-ndk:0.6`](https://hub.docker.com/r/yamsergey/bb10-ndk) (Native SDK 10.3.1; linux/386, emulated on Apple Silicon and other non-x86 hosts) |
| Engine, ported libraries | `bb10-clang++` (clang 19, libc++ 19.1.7, C++20) | an image `FROM bb10-cxx20:<pinned tag>` ([`bb10-cxx20`](https://github.com/zsavajji/bb10-cxx20), published as [`ghcr.io/zsavajji/bb10-cxx20`](https://github.com/zsavajji/bb10-cxx20/pkgs/container/bb10-cxx20)) |
| A C++14+ library the UI must call | clang, into its own `.so` that exports a C API only | the engine's image |
| Go engine | go-qnx 1.27.2, cgo through `bb10-clang` | bb10-cxx20 plus go-qnx |

**The GCC question.** The NDK has exactly two GCCs, 4.6.3 and 4.8.3 (there is no 4.8.4). 4.8.3 ships with GNU
libstdc++ only, but Qt and Cascades are built against Dinkumware. So the 4.8.3 setup compiles with the `_cpp`
variant (Dinkumware's headers) and links 4.6.3's Dinkumware library, keeping one C++ library in the process:

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

`gnu++11`, not `c++11` (strict ISO hides POSIX calls); likewise `-std=gnu99` for C. HyperBerry and Tengram run
this on the Q10 and the Classic since 2026-11-20; BBMusic and Phoenix use the default. After changing the
compiler, clean the build folder once: a `.pro` change does not make the objects stale.

Detail: `docs/03-toolchains.md`.

## 5. Building the engine

- **C++20:** CMake with `-DCMAKE_TOOLCHAIN_FILE=/opt/bb10-cxx20/share/bb10-cxx20.cmake`. The same sources also
  build on the Mac with ASan and UBSan, where the tests run (`scripts/test-engine.sh`); parsers of network
  bytes get fuzz targets.
- **The `-lz` trap is fixed in the toolchain; rely on that.** The phone's `libz.so` (and 18 other system
  libraries) export a libgcc unwinder, and with an old driver `-lz` made the first `throw` crash. Since
  bb10-cxx20 v19.1.7-4 the driver links its own unwinder first, so `-lz` and `libz.so` are fine: pin that
  release or later. Phoenix and BBMusic still link `libz.a` by path and check the binary with `readelf`,
  from before the fix; a new app needs neither.
- **Builds here, fails on the phone:** an import missing from the phone's libc links fine and fails at load.
  Check the binary's imports against the SDK's libraries (HyperBerry's `scripts/build-engine.sh`).
- **Limits:** "C" locale only; `thread_local` is emulated; add `-lbps` yourself; static libraries in dependency
  order, then zlib, then `-lsocket`.
- **Engine habits:** ignore the arguments the navigator adds (the package name, `invoke://localhost`) instead
  of exiting; data folder from `$HOME`, `--data DIR` for a test run; open a log file first (a headless
  process's stderr goes nowhere); log the version and `RLIMIT_DATA` at start.
- **Libraries:** pinned by sha256, built static into `/opt/<app>` in the app's image. OpenSSL 3.5 uses target
  `linux-armv4` and **needs the `armcap` patch**, or its CPU probes leave the thread without an FPU (SIGFPE).
  Ship Mozilla's `cacert.pem` as an asset; the phone's CA store is too old. Never link the SDK's old libssl.

Detail: `docs/04-engine-cxx20.md`, `docs/05-engine-go.md`, `docs/06-third-party-libs.md`.

## 6. Building the UI

A plain executable from a self-contained `.pro`: `TEMPLATE = app`, `CONFIG += qt warn_on release`,
`QT += declarative`, `LIBS += -lbbcascades -lbbdata -lbbsystem -lbb -lbps`. No `CONFIG += cascades10`, no
Momentics `config.pri`. In the NDK image:

```sh
source /root/bbndk/bbndk-env_10_3_1_995.sh
(cd build/app-ui && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../../app/NAME.pro && make -j"$(nproc)")
```

**A QML error makes the app quit at start with nothing on screen, or a page never open, and nothing says
why.** So every build first runs these checks:

1. `lupdate app/assets/*.qml -ts /tmp/x.ts | grep "error:"`: parses with Qt 4.8, as the phone does.
2. `grep "readonly property"`: `lupdate` accepts it, the phone refuses the page.
3. No function defined twice on a page's root object ("Duplicate method name").
4. `check-qml-handlers.py`: a handler for a signal that does not exist quits the app.
5. `skills/bb10-build-deploy/scripts/cascades-qmllint.sh`: exit 255 stops the build; read every warning.
6. On the phone, a file `data/check-qml` makes the app compile every QML document at start and log the result.

QML is Qt 4.8's: ES5 JavaScript (`var`, `function`; no `let`, arrows, template strings), no QtQuick visual
items (`Rectangle`, `MouseArea`), no `Timer` in Cascades (register `QTimer`), `onCreationCompleted` instead of
`Component.onCompleted`. In `main.cpp`: set the UTF-8 codecs for `tr()` and C strings (Qt 4 reads Latin-1),
log `QmlDocument::errors()`, and route `qDebug` to a file, since it goes nowhere on the phone.

Detail: `docs/07-ui-build.md`; for writing Cascades code, `skills/bb10-cascades` and `skills/bb10-pitfalls`.

## 7. Packaging

`bar-descriptor.xml`, then from `app/`:

```sh
blackberry-nativepackager -package ../build/NAME.bar -buildIdFile ../build/app-buildid.txt \
    bar-descriptor.xml -configuration Device-Release
unzip -p ../build/NAME.bar META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'
```

- No `-sign`, no `-devMode`.
- `<id>` never changes: a new id is another app, and uninstalling deletes `data/`.
- `-buildIdFile` gives each package the next build id.
- Every ELF (binaries, `.so`, helpers) is an asset with `type="Qnx/Elf"`, or it has no execute bit.
- Assets land in `app/native/` (read-only); `data/` is writable and is `QDir::homePath()` / `$HOME`.
- A permission left out makes its API silently do nothing.
- Env: `LD_LIBRARY_PATH=app/native/lib:/usr/lib/qt4/lib`, `CASCADES_THEME=dark?primaryColor=0x…&amp;primaryBase=0x…`.
- Icons 86, 90, 96, 110, 144 px; splash screens 720x720, 720x1280, 768x1280, 1440x1440.

Detail: `docs/08-packaging.md`.

## 8. A headless engine and its memory cap

- **Descriptor:** a second `Qnx/Elf` asset with its own `entryPointId`; an `<entryPoint>` with
  `<entryPointType>Qnx/Elf</entryPointType>` and the system permissions `_sys_run_headless`,
  `_sys_headless_nostop`, `_sys_headless_xlarge`; an `application.headless` invoke target with a custom
  `<id>.START` action. The UI starts it with an `InvokeRequest` to that target.
- **The engine must reach the navigator (BPS) within 6 s** or it is slain, and must turn `NAVIGATOR_EXIT` into
  a clean stop. A background app cannot invoke: back off, do not retry in a loop.
- **The cap:** a headless process runs under `RLIMIT_DATA` from its memory class: none 3 MiB (a Go runtime
  dies before `main`), `_sys_headless_medium` 6, `_sys_headless_large` 10, `_sys_headless_xlarge` **30 MiB**.
  Soft equals hard, so the app cannot raise it.
- **The patch** ([rlimit-patch](https://github.com/zsavajji/authman-rlimit)): authman keeps the class's limit as the text `rlm RLIMIT_DATA 31457280`.
  A root addon rewrites it to `134217728` (**128 MiB**) in the running authman's memory through
  `/proc/<pid>/as`, at every boot. Install: `/var/rootdata/installer headless-mem.zip`; log lines `[HMEM]` in
  `/var/log/installer.log`. HyperBerry and Tengram bundle the zip and install it from Settings.
- **Its caveats:** a reboot undoes it until the addon runs, about 4 minutes in; an app keeps the limit of its
  first launch in a boot until all its processes have ended, so it must close completely once after the patch.
  Editing `sys.acl`, restarting authman or loading a class at run time do not work.
- Keep the engine's peak low anyway, and log `RLIMIT_DATA` at start: that line says whether the phone is patched.
- **Start at boot:** a recurring invoke timer (`bb.action.system.TIMER_FIRED`, every 6 minutes), not the
  `STARTED` action, which also fires after every install. An unsigned app cannot run until root is up.

Detail: `docs/09-headless-service.md`.

## 9. The phone: test binaries and installs

- `ssh bb10rootx` is a root shell over USB. The phone has no `head`, `tr`, `uniq`, `printf`, `diff`, `sqlite3`
  or `grep -A`: copy files off and filter on the Mac; write lines with `echo >>`.
- **Running a test binary:** a new file runs only after `/proc/boot/pathtrust !<file>` (always with the `!`),
  and a refused file stays refused. Give each build a fresh name; [samples/phone/push.sh](samples/phone/push.sh) does it all.
  Root cannot run an installed app's binaries, and a program started over ssh cannot load a `.so` of ours.
- **Installing a `.bar` (ask first): the installer's PPS job.**

```sh
scp build/app.bar bb10rootx:/var/rootdata/appN.bar
ssh bb10rootx '
  chmod 644 /var/rootdata/appN.bar
  J=/pps/system/installer/upd/current/job.appN.bar
  (umask 000; touch $J; chmod 666 $J)
  echo "action::install"                           >> $J
  echo "package_location::/var/rootdata/appN.bar"  >> $J
  echo "extras::"                                  >> $J
  i=0; while [ $i -lt 60 ]; do grep -q "result::" $J && break; sleep 2; i=$((i+1)); done
  grep -E "result::|actual_version::" $J
  rm -f /var/rootdata/appN.bar $J'
```

  `result::success` and `actual_version::` say it worked. The version must have gone up. An install closes
  the app and its engine; data survives an upgrade. [samples/phone/install-phone.sh](samples/phone/install-phone.sh) is the script form.
- **Installing an addon:** `/var/rootdata/installer <zip>`.

Detail: `docs/10-phone-access-and-install.md`.

## 10. When something fails

| What you see | Cause |
|---|---|
| The app quits at start, nothing on screen | a QML error in the root document (section 6) |
| A page never opens | that page's QML failed to load |
| Old behaviour after an install | the version did not change, the installer kept the old app |
| The first `throw` crashes | a toolchain image older than v19.1.7-4 linked with `-lz` (section 5) |
| SIGFPE after OpenSSL starts | the `armcap` patch is missing |
| "symbol not found" only on the phone | an import the phone's libc lacks |
| The engine exits before `main`, or `std::bad_alloc` | the headless memory cap (section 8) |
| The engine exits 60 to 80 ms after launch | it took the navigator's arguments for an error |
| The engine is gone 6 s after launch | it did not reach the navigator |
| "Operation not permitted", exit 126 | the file is not trusted by `pathtrust` |
| Code that looks right does nothing | a permission is missing from the descriptor |

Before reading a log, check its time and that it comes from the build in question.
Detail: `docs/11-testing-and-debugging.md`.

## 11. Video, audio, design, skills

**Video and audio** (`docs/12-video-and-audio.md`):

- **Playing a video:** `MediaPlayer` into a `ForeignWindowControl`, from a complete local file. It does not
  stream, and it cannot seek a fragmented MP4, so download first and remux if needed (Phoenix's engine does).
- **Decoding H.264 frames from memory** (video calls): the SDK's OpenMAX **AL** cannot do it; every route was
  tried. OpenMAX **IL** works: `dlopen("/base/lib/libOmxCore.so")`, component `OMX.qcom.video.decoder.avc`,
  headers fetched at build time, nothing linked. About 24 ms a frame at 640x480, four decoders at once, baseline
  and High profile. Copy HyperBerry's `VideoDecoder`. It lives in the UI process.
- **Encoding the camera:** the camera API's own hardware H.264 encoder (`camera_start_encode`, `-lcamapi`,
  permission `use_camera`), Annex-B out. Copy HyperBerry's `CameraEncoder`.
- **Drawing frames:** a Screen child window in NV12 at z-order -1 under a `ForeignWindowControl`; set the
  window's id string before joining the group. Over a window above Cascades, nothing Cascades draws shows.
- **PCM audio:** `libasound` opened through `audio_manager` (`-lasound -laudio_manager`). No libm call per
  sample: `lrint` per sample cost 18.6 % of a core against 5.7 % with integer math. There is no Opus on the
  phone; ship libopus.
- **Where audio lives:** call audio in the UI process (the engine's heap is capped); music in the headless
  engine, which may open the speaker and outlives the UI.
- **Now-playing:** the volume overlay, driven through the PPS object
  `/pps/services/multimedia/mediaplayer/control`, which a process with no Qt can hold.

**Design** (`docs/13-design-and-screens.md`):

- Lay out in design units (`ui.du()`), not pixels. One du is 9 px on the Q10, 8 on the Classic and the Z30,
  10 on the Z10, 12 on the Passport.
- The Q10 and the Classic have the same 720x720 pixels and different du (80x80 against 90x90): check both.
- The Passport is 1440x1440 and 120x120 du: a du layout comes out small there, and HyperBerry draws
  everything a fifth bigger on it.
- Square keyboard phones are portrait only, with no on-screen keyboard; tall phones (Z10, Z30) have one.
- The Q10 and the Z30 are OLED: default to the dark theme, pure black. On LCD phones (Classic, Passport)
  HyperBerry and Tengram default to a grey page, `#181818`.
- Name the model a claim was seen on. For any model you have no phone for, say "builds; not tried on a <model>", not "works". (The author tests on a Q10 and a Classic, so for them that covers the Passport, the Z10, Z30, Z3 and the Leap.)

**Skills** (`skills/`): `bb10-cascades` (writing Cascades code), `bb10-pitfalls` (traps with phone-verified
fixes), `bb10-ux` (the stock look and behaviour), `bb10-devices` (every model), `bb10-cxx20`,
`bb10-build-deploy` (the QML lint lives there).

## 12. Samples

`samples/` holds the reference code the docs cite (not built in this repo; each dir has a README.md with the app file each sample came from): `engine-link` (UI side of the engine link), `engine-cxx20` (C++20 engine, scripts), `engine-go` (Go engine pieces), `gate-app` (a headless engine with a one-page UI for phone gates), `docker` (image Dockerfiles), `third-party` (library build scripts and patches), `ui` (`.pro`, `main.cpp`, build and icon scripts), `descriptor` (`bar-descriptor.xml` variants), `cookbook` (Cascades excerpts), `headless` (memory-patch installer), `phone` (push, install, test drivers), `video`, `audio`, `design`, `lists`. Each doc ends with the samples it uses.

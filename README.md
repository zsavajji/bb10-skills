# base-bb10

What it takes to build a native BlackBerry 10 app in 2026, written for a coding agent that starts a new one.
Everything here was learned building four apps. This repo is self-contained: the code the docs refer to is in [samples/](samples/), and no app repo is needed:

| App | What | UI | Engine | Engine runs as |
|---|---|---|---|---|
| [hyperberry](https://github.com/zsavajji/hyperberry) | WhatsApp client | Cascades, gcc 4.8.3 | Go (go-qnx) | headless service |
| [tengram](https://github.com/zsavajji/tengram) | Telegram client | Cascades, gcc 4.8.3 (fork of HyperBerry's UI) | C++20 over TDLib | headless service |
| [bbmusic](https://github.com/zsavajji/bbytmusic) | YouTube Music client | Cascades, gcc 4.6.3 | C++20, libcurl + wolfSSL | headless service |
| [phoenix](https://github.com/zsavajji/phoenix) | Reddit client | Cascades, gcc 4.6.3 | C++20, libcurl + OpenSSL | plain child of the UI |

The toolchain is [bb10-cxx20](https://github.com/zsavajji/bb10-cxx20); the headless memory patch is [rlimit-patch](https://github.com/zsavajji/authman-rlimit).

**Giving an agent context in one file:** paste [AGENT_CONTEXT.md](AGENT_CONTEXT.md) into its prompt. It is the
short form of everything below, with a pointer to the doc for each topic.

## Facts that never change

1. **BB10 10.3.3 is the last version, forever.** Every phone runs it. No version checks, no fallbacks for
   another OS version.
2. **Every phone is rooted.** Root-only routes are product design. No "without root" fallbacks.
3. **Nothing is signed.** The signing servers died in January 2022; packages are unsigned.
4. **Everything builds in Docker, on any host** (macOS, Linux or WSL): the UI in [`yamsergey/bb10-ndk:0.6`](https://hub.docker.com/r/yamsergey/bb10-ndk) (Native SDK 10.3.1,
   `qcc`), the engine in an image made from [`bb10-cxx20`](https://github.com/zsavajji/bb10-cxx20) ([`ghcr.io/zsavajji/bb10-cxx20`](https://github.com/zsavajji/bb10-cxx20/pkgs/container/bb10-cxx20)) (clang 19, C++20).
5. **Qt and Cascades use GCC's ABI with Dinkumware's C++ library; the engine uses libc++.** The two never share
   a process, except through a C API.
6. **A headless process is capped at 30 MiB** (3 MiB without a memory class), raised to 128 MiB by the
   `rlimit-patch` addon.
7. **The installer keeps the old app when the version did not change.** Every build gets a new build id.
8. **An agent installs on the phone only when the user says so, each time.**

Details and sources: [docs/01-target-and-rules.md](docs/01-target-and-rules.md).

## The docs, in reading order

| File | Read it when |
|---|---|
| [01-target-and-rules.md](docs/01-target-and-rules.md) | First, always: the devices, the OS, what is allowed on the phone |
| [02-architecture.md](docs/02-architecture.md) | Deciding the app's shape: UI and engine, their link, who owns the data, the repo layout, the Phase 0 gates |
| [03-toolchains.md](docs/03-toolchains.md) | Setting up the Docker images; choosing gcc 4.6.3, gcc 4.8.3 or clang for a piece of code |
| [04-engine-cxx20.md](docs/04-engine-cxx20.md) | Writing or building a C++20 engine (CMake, the link line, the `-lz` trap and its fix, host tests) |
| [05-engine-go.md](docs/05-engine-go.md) | Writing or building a Go engine |
| [06-third-party-libs.md](docs/06-third-party-libs.md) | Adding a library: OpenSSL, curl, wolfSSL, opus, libwebp, TDLib, SQLCipher |
| [07-ui-build.md](docs/07-ui-build.md) | Building the Cascades UI: the `.pro` file, the QML checks that run before a build, translations |
| [08-packaging.md](docs/08-packaging.md) | Writing `bar-descriptor.xml` and making the `.bar` |
| [09-headless-service.md](docs/09-headless-service.md) | The engine has to outlive the UI: entry points, invoke targets, the memory cap and its patch, start at boot |
| [10-phone-access-and-install.md](docs/10-phone-access-and-install.md) | Touching the phone: root SSH, running a test binary, installing a `.bar` or an addon |
| [11-testing-and-debugging.md](docs/11-testing-and-debugging.md) | Testing off and on the phone, reading logs, working out why an app quits |
| [12-video-and-audio.md](docs/12-video-and-audio.md) | Playing video, decoding or encoding H.264 in hardware (the OpenMAX AL research and the OpenMAX IL route that works), the camera, drawing frames, PCM audio in and out |
| [13-design-and-screens.md](docs/13-design-and-screens.md) | Designing a page: design units (du) and their size per model, square and tall screens, the Passport's larger screen, OLED and LCD panels and the theme, icons and splash screens, how the four apps lay out for every phone |

## The skills

Six Claude Code skills, in `skills/`. They hold the platform knowledge that is not about the
build pipeline. Link or copy them into `~/.claude/skills/` or a project's `.claude/skills/`.

| Skill | Covers |
|---|---|
| [bb10-cascades](skills/bb10-cascades/SKILL.md) | Writing Cascades code: QML rules, C++ and QML, lists, layout, system integration; a starter template |
| [bb10-pitfalls](skills/bb10-pitfalls/SKILL.md) | Traps with phone-verified fixes: silent quits, focus, the keyboard, menus, native windows, lifecycle |
| [bb10-ux](skills/bb10-ux/SKILL.md) | How a BB10 app should look and behave; the official UI guidelines |
| [bb10-devices](skills/bb10-devices/SKILL.md) | Every model's screen, design units, input, hardware; per-device assets |
| [bb10-cxx20](skills/bb10-cxx20/SKILL.md) | Using the `bb10-cxx20` toolchain |
| [bb10-build-deploy](skills/bb10-build-deploy/SKILL.md) | Build, package, lint, install, failure signatures; its `scripts/` holds the QML lint (`cascades-qmllint.sh`) and the signal-handler check (`check-qml-handlers.py`) |

## Samples

[samples/](samples/) holds the reference code the docs cite; it is not built in this repo. Each top-level dir has a README.md saying which app file each sample came from.

| Dir | Holds |
|---|---|
| [engine-link](samples/engine-link/) | The UI side of the engine link: loopback TCP with a token (headless), stdio child process |
| [engine-cxx20](samples/engine-cxx20/) | C++20 engine pieces: CMake, `main` for a child and a headless service, server, build/test/fuzz scripts |
| [engine-go](samples/engine-go/) | Go engine pieces: build and test scripts, `go.mod`, memory limit, navigator |
| [docker](samples/docker/) | The Dockerfiles for the engine images, with the OpenSSL QNX patch |
| [third-party](samples/third-party/) | Scripts and patches that build libraries (TDLib, libwebp, SQLCipher, ...) |
| [ui](samples/ui/) | `.pro` files, `main.cpp`, logging, build and icon scripts |
| [descriptor](samples/descriptor/) | `bar-descriptor.xml` for a child engine, a headless engine and a full app |
| [cookbook](samples/cookbook/) | Cascades C++ and QML excerpts: invoke, share, SQLite, settings, cover |
| [headless](samples/headless/) | The memory-patch installer: `MemoryPatch` and the root helper |
| [phone](samples/phone/) | Push, install and test-driver scripts for the phone |
| [video](samples/video/) | Hardware H.264 decode and encode, camera, drawing, remux |
| [audio](samples/audio/) | PCM audio, call audio, proximity, now-playing |
| [design](samples/design/) | Passport zoom, tap area, cover and page QML |
| [lists](samples/lists/) | C++ list rows |

## A new app, step by step

1. Read 01 and 02. Decide: is there an engine, in C++20 or Go, and does it outlive the UI (headless service) or
   die with it (child process)?
2. List the risky parts and prove each on the phone with the smallest program that can fail (the Phase 0 gates,
   02). A failed gate changes the plan, not the code.
3. Lay the repo out as Phoenix's (02): `app/`, `engine/`, `docker/`, `scripts/`, `docs/protocol.md`, `build/`
   git-ignored.
4. Write `docker/Dockerfile`: a pinned `bb10-cxx20` plus each library, pinned by sha256, built static (03, 06).
5. Engine: host build with ASan and UBSan and its tests first, then the cross build (04), or the Go build with the import check (05).
6. UI: a plain executable from a self-contained `.pro`; parse the QML with `lupdate`, lint it, check the signal
   handlers, before every build (07). Write the UI with the cascades, pitfalls and ux skills open.
7. `bar-descriptor.xml`: an id that never changes, the build id from `-buildIdFile`, every binary as `Qnx/Elf`
   (08). A headless engine also needs its entry point, its memory class and the memory patch (09).
8. Build the `.bar`, check `Package-Version` in its manifest, and hand it over or, when the user says so,
   install it over root SSH (10).
9. Read the logs off the phone before concluding anything (11). Say "builds" until the phone confirms "works".

## Keeping this current

Each doc ends with the samples it uses. When an app teaches something new, change the doc of that
concern, keep the citation, and change [AGENT_CONTEXT.md](AGENT_CONTEXT.md) if the short form says otherwise.
[BB10_CONTEXT.md](BB10_CONTEXT.md) is the Cascades API cookbook; AGENT_CONTEXT.md is the file to paste.

# samples

Code the docs point at, extracted from the four apps this knowledge base was learned from, so the repo
stands on its own: nothing here needs an app repo checked out.

- The files are **samples to read and copy from, not a project**: nothing in this folder is built or
  tested in this repo. They keep the app's own names and ids (`com.zsavajji.bbmusic`, `phoenixd`).
- Each folder's `README.md` says, per file, which app and path it came from, at which commit, and whether
  it is verbatim, an excerpt (cuts are marked `[...] app logic left out`) or adapted (marked
  `base-bb10: path changed`).
- **Licence:** MIT, as the rest of this repo ([LICENSE](../LICENSE)); the apps the files come from are
  the same author's. The apps themselves are published under their own licences (HyperBerry, Tengram and
  BBMusic: GPL-3.0).

| Folder | What it holds | Doc |
|---|---|---|
| [engine-link/](engine-link/) | the UI's link to the engine: loopback TCP with a token (headless engine), or a child process's stdin and stdout | [02](../docs/02-architecture.md), [09](../docs/09-headless-service.md) |
| [engine-cxx20/](engine-cxx20/) | a C++20 engine: CMake file, `main` for a child and for a headless service, the token server, the navigator, build, test and fuzz scripts, a protocol doc | [04](../docs/04-engine-cxx20.md) |
| [gate-app/](gate-app/) | a gate app: a small headless engine plus a one-page UI, for testing what needs an installed `.bar` (headless launch, token handshake, the app swiped away); gate programs run from a root shell | [02](../docs/02-architecture.md) section 8 |
| [engine-go/](engine-go/) | a Go engine: build and test scripts, patching dependencies, CA roots, the navigator through cgo, the memory limit | [05](../docs/05-engine-go.md) |
| [docker/](docker/) | build images: OpenSSL and curl, wolfSSL, curl and opus, go-qnx, TDLib; the OpenSSL `armcap` patch | [03](../docs/03-toolchains.md), [06](../docs/06-third-party-libs.md) |
| [third-party/](third-party/) | fetch and build scripts for TDLib, calls, rlottie as a C-API `.so`, libwebp, qrcodegen, the SQLCipher driver, the emoji font; the TDLib patches | [06](../docs/06-third-party-libs.md) |
| [ui/](ui/) | the Cascades UI: `.pro` files (gcc 4.6.3 and 4.8.3), `main.cpp`, the UI log, the build script with its QML checks, icon generators | [07](../docs/07-ui-build.md) |
| [descriptor/](descriptor/) | `bar-descriptor.xml`: engine as a child, engine as a headless service, the full set (Hub, share, timer, `.so`, helper) | [08](../docs/08-packaging.md) |
| [launcher/](launcher/) | the UI as a shared object started through BB10's Cascades launcher (`Qnx/Cascades`): the `.pro`, the descriptor, the lines of `main.cpp` it needs | [14](../docs/14-cascades-launcher.md) |
| [cookbook/](cookbook/) | Cascades API recipes: share, query targets, clipboard, lifecycle, theme, SQLite, Active Frame, a card | [BB10_CONTEXT](../BB10_CONTEXT.md) |
| [headless/](headless/) | installing the memory-cap addon from the app: `MemoryPatch` and its root helper | [09](../docs/09-headless-service.md) |
| [phone/](phone/) | scripts for the phone: push a test binary, install a `.bar`, measure, drive the engine | [10](../docs/10-phone-access-and-install.md), [11](../docs/11-testing-and-debugging.md) |
| [video/](video/) | the OpenMAX IL H.264 decoder, the Screen window, the camera encoder, the video viewer, the OpenMAX gate tools, the MP4 remuxer | [12](../docs/12-video-and-audio.md) |
| [audio/](audio/) | PCM through libasound and audio_manager (calls, music), proximity, recording, now-playing over PPS | [12](../docs/12-video-and-audio.md) |
| [design/](design/) | the Passport zoom, an Active Frame for every size, a page that rises by swapping the scene, the `TapArea` attempt | [13](../docs/13-design-and-screens.md) |
| [lists/](lists/) | list rows drawn in C++ with a `ListItemProvider` | [13](../docs/13-design-and-screens.md) section 6 |

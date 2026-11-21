# BlackBerry 10 App Development — Agent Context Primer

> A Cascades API cookbook and skeleton. For the short project context paste [AGENT_CONTEXT.md](AGENT_CONTEXT.md);
> the toolchains, build and install are in [docs/](docs/) (start from [README.md](README.md)).

## Project constraints (read first)

- **Goal:** build a native BlackBerry 10 application using **Cascades**.
- **Builds run in Docker**, on a Mac, Linux or WSL host: the NDK image and the bb10-cxx20 image. See
  [docs/03-toolchains.md](docs/03-toolchains.md) and [docs/07-ui-build.md](docs/07-ui-build.md).
- **Signing is a non-issue.** BlackBerry's code-signing / debug-token servers are dead
  (legacy services ended Jan 2022), which normally blocks deployment. The user's device
  is **rooted and runs unsigned `.bar` files**. Skip ALL signing setup:
  no `blackberry-keytool`, no `blackberry-debugtokenrequest`, no author certs, no
  `bbidtoken.csk`. Package with `-devMode` and deploy directly.

## Canonical reference: Tengram (READ IT FIRST)

[tengram](https://github.com/zsavajji/tengram) is its own repo, checked out beside this one on the author's machine as `../tengram` (not
inside base-bb10). It is a real, complete BB10 Cascades app (a Telegram client) with a headless C++20 engine, and
the reference for how these apps are built. Prefer copying its patterns over any generic web guide. Layout:

```
tengram/
├── app/                      # the Cascades UI
│   ├── tengram.pro           # plain qmake executable: QT, LIBS, gcc 4.8.3 + gnu++11
│   ├── bar-descriptor.xml    # permissions, invoke targets, env vars (LD_LIBRARY_PATH, CASCADES_THEME)
│   ├── src/                  # C++: main.cpp, EngineLink (talks to the engine), models, controllers, cards
│   ├── assets/               # one .qml per page, cover.qml, small .js helpers, images, fonts
│   ├── helper/, translations/, tests/
├── engine/                   # tengramd, the headless engine (C++20 over TDLib), a separate process
├── calls/                    # the calls library
├── lib/                      # third-party sources: libwebp, omxil, qrcodegen, qtsqlcipher
├── scripts/, docker/         # build and install scripts, the build images
└── docs/, NOTES.md, CLAUDE.md
```

Pattern (confirmed in `app/src/main.cpp`): C++ owns the models and controllers and exposes each to QML with
`document->setContextProperty("chats", &chats)` and the like; the QML pages call `controller.method(...)` and bind
to `Q_PROPERTY` values and list models. The UI never does network work itself: `EngineLink` talks to the engine
process ([docs/02-architecture.md](docs/02-architecture.md)).

## BB10 API cookbook — real techniques (Tengram `app/src/` unless noted)

Concrete, hard-to-find-now recipes, with the exact classes and where they live:

| Task | How | Where |
|------|-----|-------|
| Share (target picker) | `bb::system::InvokeManager` + `InvokeRequest` (`action=bb.action.SHARE`, `mimeType`, `setData`) | `OpenRequests.cpp` |
| Enumerate targets | `InvokeQueryTargetsRequest`/`Reply`, iterate `actions()`→`targets()` | `MapChooser.cpp` |
| Open URL in browser | `InvokeRequest` `target="sys.browser"`, `action="bb.action.OPEN"` | `LinkPreview.cpp`, `MapChooser.cpp` |
| Compose email | `InvokeRequest` `target="sys.pim.uib.email.hybridcomposer"`, `mailto:` URI | no Tengram example |
| Clipboard | `bb::system::Clipboard`, `insert("text/plain", bytes)` | `ChatController.cpp` |
| Active Frame | a **separate `cover.qml`** (a `SceneCover`), set as the app's cover | `assets/cover.qml`, `assets/main.qml` |
| App lifecycle (bg/fg) | `Application` signals `thumbnail()` / `fullscreen()` | `EngineLink.cpp` |
| Media/volume keys | `bb::multimedia::MediaKeyWatcher` (`VolumeUp/Down/PlayPause`), `shortPress`/`longPress`; `eventFilter` fallback for `QKeyEvent` | no Tengram example |
| Accelerometer / tilt | QtMobility sensors (`CONFIG+=mobility; MOBILITY+=sensors`), a small `OrientationSensor` class | no Tengram example |
| Runtime dark/bright theme | `Application::themeSupport()->setVisualStyle(VisualStyle::Dark/Bright)` | `Settings.cpp` |
| App version at runtime | `bb::ApplicationInfo().version()` (reads bar-descriptor; bump the version there only) | `main.cpp` |
| Screen size | `bb::device::DisplayInfo().pixelSize()`, bucket by resolution | `main.cpp`, `CardStyle.h` |
| Shared/exported files | `QDir::homePath()+"/shared/..."` (visible in the file manager and on a PC); needs `access_shared` | `MediaOpener.cpp`, `main.cpp` |
| SQLite from QML | `import QtQuick 1.0` + `openDatabaseSync(...)` (LocalStorage/WebSQL) | no Tengram example |
| SQLite from C++ | `QSqlDatabase::addDatabase("QSQLITE")` (`QT += sql`); QML LocalStorage DBs live at an opaque path, so C++ must probe for the file | `ChatsDb.cpp` |
| File read/write helper | a header-only `FileIO : QObject` with `Q_INVOKABLE read/write`, exposed to QML (QML cannot do raw file I/O) | no Tengram example |

Permissions these need (add to bar-descriptor): `run_native` (always), `access_shared`,
`use_camera`, and for a background/Active-Frame app: `run_when_backgrounded`,
`_sys_run_headless`, `_sys_headless_nostop`. Tengram's `app/bar-descriptor.xml` has a full set.

## Platform stack (three layers)

1. **QNX Neutrino** — real microkernel RTOS. Your app is a genuine POSIX process that
   keeps running in the background (this is why Active Frames can show live content).
2. **Qt 4.8** — the object model underneath. You get `QObject`, signals/slots, `QString`,
   `QAbstractItemModel`, `QNetworkAccessManager`, the event loop.
   - **It is Qt 4.8, NOT Qt 5.** No QtQuick 2 and no QtQuick *visuals* (below). The NDK has two compilers:
     **GCC 4.6.3** (default, C++98 plus part of C++11) and **GCC 4.8.3** with `-std=gnu++11` (`override`, `final`,
     fuller C++11; Tengram and HyperBerry use it). Neither has C++14/17 (that is the engine's clang, a separate
     process). Use old-style `connect(sender, SIGNAL(...), recv, SLOT(...))`. Setup: [docs/03-toolchains.md](docs/03-toolchains.md).
3. **Cascades** — BlackBerry's UI toolkit on top of Qt. It has its OWN QML engine
   (`import bb.cascades 1.x`) rendering on a separate thread.
   - **QtQuick *visual* items do NOT render in the Cascades scene.** No `Rectangle`,
     `Item`, `MouseArea` in your UI tree — use `Container`, `Control`, `Page`, layouts.
   - BUT Cascades sits on **QtDeclarative (QtQuick 1)**, so `import QtQuick 1.0`
     *non-visual* elements work (`QtObject` singletons for logic, the JS
     `openDatabaseSync(...)` LocalStorage/WebSQL API). So the rule is "no QtQuick *visuals*", not "no QtQuick at all".
   - You cannot mix Qt5 code with Cascades APIs. Stay on the Cascades/Qt4.8 path.

## Toolchain

- **BlackBerry Native SDK / NDK 10.3.1** in the Docker image
  [`yamsergey/bb10-ndk:0.6`](https://hub.docker.com/r/yamsergey/bb10-ndk); no IDE. Images and compiler choice:
  [docs/03-toolchains.md](docs/03-toolchains.md).
- Key tools: `qcc` (ARM cross-compiler), `qmake`, `blackberry-nativepackager` (make `.bar`). Ignore the signing
  tools per constraints above.
- Env: the image's `bbndk-env_10_3_1_995.sh` sets `QNX_HOST`, `QNX_TARGET` and `PATH`.

## Idiomatic Cascades app = QML for UI, C++ for logic, bridged via QmlDocument

Do NOT build the UI in pure C++ (it compiles but is verbose and non-idiomatic; some
archived samples do this — don't copy that style). Standard skeleton:

### `src/main.cpp`
```cpp
#include <bb/cascades/Application>
#include "applicationui.hpp"
using namespace bb::cascades;

int main(int argc, char **argv)
{
    Application app(argc, argv);   // bb::cascades::Application is the REAL entry point
    new ApplicationUI(&app);       // parented to app
    return Application::exec();
}
```

### `src/applicationui.hpp`
```cpp
#include <QObject>
namespace bb { namespace cascades { class Application; }}

class ApplicationUI : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationUI(bb::cascades::Application *app);
    Q_INVOKABLE QString greet(const QString &name);  // callable from QML
signals:
    void dataUpdated(const QString &value);          // C++ -> QML
};
```

### `src/applicationui.cpp`
```cpp
#include "applicationui.hpp"
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>
using namespace bb::cascades;

ApplicationUI::ApplicationUI(Application *app) : QObject(app)
{
    QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(this);
    qml->setContextProperty("_app", this);   // THE bridge: expose C++ to QML as _app
    AbstractPane *root = qml->createRootObject<AbstractPane>();
    app->setScene(root);
}

QString ApplicationUI::greet(const QString &name) { return "Hello, " + name; }
```

### `assets/main.qml`
```qml
import bb.cascades 1.4

Page {
    Container {
        layout: StackLayout {}
        Label { id: hello; text: qsTr("Tap the button")
                textStyle.base: SystemDefaults.TextStyles.BigText }
        Button { text: qsTr("Greet")
                 onClicked: hello.text = _app.greet("BlackBerry") }  // QML -> C++
    }
    cover: SceneCover {                        // live multitasking thumbnail
        content: Container { layout: DockLayout {}
            Label { text: "MyApp"; horizontalAlignment: HorizontalAlignment.Center } }
    }
}
```

## C++ <-> QML bridge (used constantly)

| Need | Tool |
|------|------|
| Expose one C++ object to QML | `qml->setContextProperty("_app", this)` |
| Let QML instantiate a C++ type | `qmlRegisterType<T>("com.example",1,0,"T")` + `import com.example 1.0` |
| C++ pushes data to QML | C++ `signal` -> QML `on<Signal>` / `Connections {}` |
| QML calls C++ | mark method `Q_INVOKABLE` or make it a `public slot` |
| Bindable C++ value | `Q_PROPERTY(... READ ... NOTIFY ...)` |
| List data | C++ `bb::cascades::DataModel` / `GroupDataModel` (SQLite+sort+sections) -> QML `ListView` |

## Core UI building blocks

- **Root panes:** `Page` (one screen), `NavigationPane` (push/pop stack; gives the
  right-edge back-swipe for free), `TabbedPane` (top-level tabs). Pick one as scene root.
- **Layouts:** `StackLayout` (v/h), `DockLayout` (edge/center align + layering),
  `AbsoluteLayout` (rare). NO anchors — it's box layout.
- **Controls:** `Label`, `Button`, `TextField`, `TextArea`, `ImageView`, `ListView`,
  `ToggleButton`, `Slider`, `DropDown`, `WebView`.
- **Chrome:** `TitleBar`, `ActionItem`s in `Page.actions` (bottom Action Bar, ~4 on-bar +
  overflow), `Sheet` (full-screen modal — BB10 uses these instead of dialogs), `Menu`.
- **Assets:** reference with `asset:///images/foo.png`; density variants auto-selected.

## BB10 native UX patterns (what makes an app feel native)

- **Hub:** unified inbox. Post notifications via `bb::platform::Notification`
  (`setTitle/setBody/setInvocationRequest/notify()`); declare an `<invoke-target>` in
  `bar-descriptor.xml` to be openable/shareable.
- **Active Frames:** live minimized-app thumbnails via `SceneCover` (C++
  `Application::instance()->setCover(...)` or QML `cover:`). Keep to 1–3 elements,
  dark background, ~280x210 pt.
- **Peek/Flow:** gesture navigation. `NavigationPane` handles right-edge back-swipe;
  `popTransitionEnded` signal fires on back.

## `bar-descriptor.xml` — easy to forget

Manifest for packaging. **Permissions must be declared or APIs silently fail at runtime.**
`run_native` is REQUIRED for every native app (Tengram uses these):
```xml
<permission system="true">run_native</permission>       <!-- REQUIRED for native apps -->
<permission>access_internet</permission>
<permission system="true">_sys_run_headless</permission> <!-- background/headless -->
<permission>access_shared</permission>                    <!-- shared files -->
<permission>use_camera</permission>
<!-- others as needed: access_location_services, record_audio,
     read_device_identifying_information -->
```
Also sets entry point (`<asset entry="true">`), icon (multiple sizes), splash screens
(per resolution), orientation, and `<invoke-target>`s (Hub / share integration).
It can also inject env vars, incl. **runtime theming** (Tengram does this):
```xml
<env var="LD_LIBRARY_PATH" value="app/native/lib:/usr/lib/qt4/lib"/>
<env var="CASCADES_THEME" value="dark?primaryColor=0x7289DA&amp;primaryBase=0x7289DA"/>
```

### Entry asset
The entry point is a plain ELF executable built by qmake (`<asset entry="true">` points at it), so `main()` is an
ordinary `int main(...)`. See [docs/08-packaging.md](docs/08-packaging.md).

## Build & deploy (rooted device, no signing)

Build and package with the app's `scripts/build-app.sh` in the NDK image ([docs/07-ui-build.md](docs/07-ui-build.md),
[docs/08-packaging.md](docs/08-packaging.md)). Install the unsigned `.bar` over root SSH or with Sachesi or DBBT:
[docs/10-phone-access-and-install.md](docs/10-phone-access-and-install.md). Rooting: https://bb10.root.sx.

## Linking libraries & third-party C/C++ code

- **System libs** via `.pro` `LIBS +=` / `QT +=`. Tengram uses `QT += declarative network sql gui` and
  `LIBS += -lbbcascades -lbbdevice -lbbdata -lbbsystem -lbb ...` (see `app/tengram.pro`).
  Rule of thumb: `-lbb*` = BlackBerry platform libs.
- **Third-party libs**: pinned sources under `lib/` (libwebp, qrcodegen, omxil, qtsqlcipher) compiled into the app by
  the `.pro`, or cross-compiled as static libraries: [docs/06-third-party-libs.md](docs/06-third-party-libs.md).
- Pass raw C flags to `qcc` with the `-Wc,` prefix, e.g. `QMAKE_CFLAGS += -Wc,-std=gnu99`.
- `.pro` convention: a plain qmake project that builds an executable (template in
  [docs/07-ui-build.md](docs/07-ui-build.md)).

## Standard project layout

```
MyApp/
├── bar-descriptor.xml     # manifest: permissions, icon, entry point, invoke targets
├── MyApp.pro              # qmake project file
├── src/
│   ├── main.cpp
│   ├── applicationui.hpp
│   └── applicationui.cpp
├── assets/
│   └── main.qml           # + other .qml, images/
└── translations/
    └── MyApp.ts           # qsTr() strings
```

## Reference resources

- **[Tengram](https://github.com/zsavajji/tengram)** — the canonical app. Read it before any web guide;
  its layout is above.
- **blackberry/Cascades-Samples** (github.com/blackberry/Cascades-Samples) — 100+ real,
  buildable sample apps. Best learning source; clone it. (Archived read-only, still cloneable.)
- **Cascades API reference:** `developer.blackberry.com/native/documentation/cascades/`
  is dead — use the Wayback Machine (web.archive.org).
- **Qt 4.8 docs:** doc.qt.io/archives/qt-4.8/ — for everything under Cascades. NOTE: any
  "qt-docs" MCP tool is Qt 6 and will NOT match BB10's Qt 4.8 APIs; use the 4.8 archive.
- **Community archive:** github.com/tranhoangtu-it/Everything-Blackberry (guides on setup,
  sideloading, UI patterns) and bb10.root.sx (rooting / unsigned-install details).

## Common mistakes to avoid

- Using QtQuick *visual* items (`Rectangle`, `Item`, `MouseArea`) in the UI tree — not
  available. (Non-visual `import QtQuick 1.0` `QtObject`/`openDatabaseSync` IS fine.)
- Building UI in pure C++ instead of QML + QmlDocument bridge.
- Assuming Qt 5 / modern C++ in the UI — it's Qt 4.8 and GCC 4.6.3 or 4.8.3 (C++11 at most).
- Forgetting to declare a `<permission>` — the API just fails silently.
- Building outside the Docker images — use the build scripts ([docs/03-toolchains.md](docs/03-toolchains.md)).
- Spending any effort on signing/debug tokens — the device is rooted; skip it.

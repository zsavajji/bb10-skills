# BlackBerry 10 App Development — Agent Context Primer

> **Superseded (2026-11-21). Paste [AGENT_CONTEXT.md](AGENT_CONTEXT.md) instead, and start from [README.md](README.md).** This primer is from 2026-08, before
> the Docker toolchains. Wrong today: "this Mac cannot build" and "build on the Windows box" (everything
> builds from the Mac in Docker, see [docs/03-toolchains.md](docs/03-toolchains.md)); Momentics, `config.pri`
> and the `Device-Debug`/`.so` configurations (the apps are plain executables built by qmake, see
> [docs/07-ui-build.md](docs/07-ui-build.md)); Sachesi as the only install route
> (see [docs/10-phone-access-and-install.md](docs/10-phone-access-and-install.md)). Still useful: the API
> cookbook table and the Cascades skeleton below, which [skills/bb10-cascades](skills/bb10-cascades/SKILL.md)
> covers in more depth.

> Paste this as the base prompt / context when starting work on this BB10 project.
> It captures the platform facts, the toolchain, the idiomatic Cascades patterns,
> and the project-specific constraints. Verified 2026-08.

## Project constraints (read first)

- **Goal:** build a native BlackBerry 10 application using **Cascades**.
- **This Mac cannot build BB10 apps.** The Momentics SDK runs on Windows (also old
  Linux/macOS). Building/compiling happens on a separate Windows box. Do NOT try to
  compile here — write code, scaffold, and reason about it; leave the build to the user.
- **Signing is a non-issue.** BlackBerry's code-signing / debug-token servers are dead
  (legacy services ended Jan 2022), which normally blocks deployment. The user's device
  is **rooted and runs unsigned `.bar` files**. Skip ALL signing setup:
  no `blackberry-keytool`, no `blackberry-debugtokenrequest`, no author certs, no
  `bbidtoken.csk`. Package with `-devMode` and deploy directly.
- Working dir: `/Users/andreafaggin/own-projects/bb10`.

## Canonical reference: the `bbcord/` project in this repo (READ IT FIRST)

[bbcord](https://github.com/michioxd/bbcord) is a **real, complete, structured BB10 Cascades app** (an unofficial Discord
client) sitting in this repo. It is the best possible reference — a working example of
exactly this kind of app, with real conventions. Prefer copying its patterns over any
generic web guide. Its architecture:

```
bbcord/
├── BBCord.pro           # APP_NAME + LIBS + QT modules; include(config.pri)
├── config.pri           # Momentics-generated: source globbing, per-config DESTDIR
├── bar-descriptor.xml   # real permissions, icons, splashscreens, env-var theming
├── src/
│   ├── main.cpp         # Q_DECL_EXPORT int main; `ApplicationUI appui;` on the stack
│   ├── applicationui.*  # owns all controllers; wires them to QML via setContextProperty
│   ├── core/            # non-UI: Client, AppStore, models/, cache workers (QThread)
│   ├── ui/              # one <Screen>Controller (QObject) per screen, exposed to QML
│   └── utils/           # e.g. MarkdownParser
├── assets/*.qml         # one .qml per screen (MainPage, LoginPage, ChatCard, ...)
└── third_party/         # git submodules: openssl, websocketpp, asio, jsoncpp,
                         # libwebsockets, mongoose (cross-compiled to static .a per target)
```

Pattern (confirmed in its code): each screen is a C++ `*Controller : QObject` with
`Q_INVOKABLE` methods, `Q_PROPERTY` data models (`ArrayDataModel`/`DataModel`), and
`Q_SLOTS`; `ApplicationUI` constructs them all and exposes each via
`qml->setContextProperty("mainPageController", m_mainPageController)` etc. QML screens
call `controllerName.method(...)` and bind to `controllerName.someProperty`.

## Second reference: `SmartList10/` (a contrasting architecture)

[SmartList10](https://github.com/BBerrylife/SmartList10) is a second complete Cascades app (a checklist/task manager). It shows
the **opposite** split from bbcord and is worth studying for that contrast:

- **Logic lives in QML/JS**, not C++ controllers: `AppLogic.qml`, `AppStorage.qml`,
  `ModelManager.qml` are `QtObject` singletons doing the work. C++ is thin — one big
  `ApplicationUI` exposed as a single `app` context property, plus small helpers
  (`FileIO`, `OrientationSensor`).
- **No submodules / no third_party** — pure Qt4.8 + Cascades + QtMobility sensors.
- `.pro`: `QT += core declarative sql`, `CONFIG += mobility`, `MOBILITY += sensors`,
  `LIBS += -lbbdata -lbbsystem -lbb -lbbdevice -lbbmultimedia`.

Takeaway: both styles are valid. C++-heavy (bbcord) suits complex networking/state;
QML-heavy (SmartList10) suits UI-centric apps. Pick per app, don't mix dogmatically.

## BB10 API cookbook — real techniques (see SmartList10 unless noted)

Concrete, hard-to-find-now recipes, with the exact classes and where they live:

| Task | How | Where |
|------|-----|-------|
| Share text (target picker) | `bb::system::InvokeManager` + `InvokeRequest` (`action=bb.action.SHARE`, `mimeType=text/plain`, `setData`) | `applicationui.cpp` `shareText()` |
| Enumerate share targets | `InvokeQueryTargetsRequest`/`Reply`, iterate `actions()`→`targets()` | `queryShareTargets()` |
| Open URL in browser | `InvokeRequest` `target="sys.browser"`, `action="bb.action.OPEN"` | bbcord `openLink()` |
| Compose email | `InvokeRequest` `target="sys.pim.uib.email.hybridcomposer"`, `mailto:` URI | `invokeEmail()` |
| Clipboard | `bb::system::Clipboard`, `insert("text/plain", bytes)` | `copyToClipboard()` |
| Interactive Active Frame | Load a **separate `cover.qml`** as its own `QmlDocument`, `createRootObject<SceneCover>()`, `Application::setCover()` | `applicationui.cpp` ctor |
| App lifecycle (bg/fg) | `Application` signals `thumbnail()` / `fullscreen()`; `Application::minimize()` | ctor + `onThumbnailed()` |
| Media/volume keys | `bb::multimedia::MediaKeyWatcher` (`VolumeUp/Down/PlayPause`), `shortPress`/`longPress`; `eventFilter` fallback for `QKeyEvent` | `connectMediaKeyWatchers()` |
| Accelerometer / tilt | QtMobility sensors (`CONFIG+=mobility; MOBILITY+=sensors`), custom `OrientationSensor` | `src/OrientationSensor.*` |
| Runtime dark/bright theme | `Application::themeSupport()->setVisualStyle(VisualStyle::Dark/Bright)`, **guard with** `#if BBNDK_VERSION_AT_LEAST(10,3,0)` (`<bbndk.h>`) | `setDarkTheme()`, `main.cpp` |
| App version at runtime | `bb::ApplicationInfo().version()` (reads bar-descriptor; bump version there only) | `appVersion()` |
| Pick asset by screen size | `bb::device::DisplayInfo().pixelSize()`, bucket by resolution | ctor |
| Shared/exported files | `QDir::homePath()+"/shared/documents/..."` (visible in file manager/PC); needs `access_shared` | `downloadsPath()` |
| SQLite from QML | `import QtQuick 1.0` + `openDatabaseSync(...)` (LocalStorage/WebSQL) | `AppStorage.qml` |
| SQLite from C++ | `QSqlDatabase::addDatabase("QSQLITE")` (`QT += sql`); note QML LocalStorage DBs live at an opaque path, so C++ must probe for the file | `main.cpp` (reads theme pre-UI) |
| File read/write helper | header-only `FileIO : QObject` with `Q_INVOKABLE read/write`, exposed to QML (QML can't do raw file I/O) | `src/fileio.h` |

Permissions these need (add to bar-descriptor): `run_native` (always), `access_shared`,
`use_camera`, and for a background/Active-Frame app: `run_when_backgrounded`,
`_sys_run_headless`, `_sys_headless_nostop`.

## Platform stack (three layers)

1. **QNX Neutrino** — real microkernel RTOS. Your app is a genuine POSIX process that
   keeps running in the background (this is why Active Frames can show live content).
2. **Qt 4.8** — the object model underneath. You get `QObject`, signals/slots, `QString`,
   `QAbstractItemModel`, `QNetworkAccessManager`, the event loop.
   - **It is Qt 4.8, NOT Qt 5.** No `QtQuick`. Compiler is **GCC 4.6.3** → C++11 only
     partially, no C++14/17. Use old-style `connect(sender, SIGNAL(...), recv, SLOT(...))`.
3. **Cascades** — BlackBerry's UI toolkit on top of Qt. It has its OWN QML engine
   (`import bb.cascades 1.x`) rendering on a separate thread.
   - **QtQuick *visual* items do NOT render in the Cascades scene.** No `Rectangle`,
     `Item`, `MouseArea` in your UI tree — use `Container`, `Control`, `Page`, layouts.
   - BUT Cascades sits on **QtDeclarative (QtQuick 1)**, so `import QtQuick 1.0`
     *non-visual* elements work: `QtObject` singletons for logic, and the JS
     `openDatabaseSync(...)` LocalStorage/WebSQL API (SmartList10's `AppStorage.qml`
     does exactly this). So the rule is "no QtQuick *visuals*", not "no QtQuick at all".
   - You cannot mix Qt5 code with Cascades APIs. Stay on the Cascades/Qt4.8 path.

## Toolchain

- **Momentics IDE** (Eclipse-based) + **BlackBerry Native SDK / NDK 10.3.1**
  (recommended version for max device compatibility). Official servers are offline;
  obtain from Internet Archive (`archive.org/details/native-SDK-for-blackberry10`,
  `archive.org/details/bbdevtools`) or community mirrors.
- Key CLI tools (in `$QNX_HOST/usr/bin`): `qcc` (ARM cross-compiler), `qmake`,
  `blackberry-nativepackager` (make `.bar`), `blackberry-deploy` (install `.bar`).
  Ignore the signing tools per constraints above.
- Env: `QNX_HOST`, `QNX_TARGET` must be set; add `$QNX_HOST/usr/bin` to PATH.

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
`run_native` is REQUIRED for every native app (bbcord uses these):
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
It can also inject env vars, incl. **runtime theming** (bbcord does this):
```xml
<env var="LD_LIBRARY_PATH" value="app/native/lib:/usr/lib/qt4/lib"/>
<env var="CASCADES_THEME" value="dark?primaryColor=0x7289DA&amp;primaryBase=0x7289DA"/>
```

### Per-configuration entry assets (note: release device build is a `.so`)
```xml
<configuration name="Device-Debug">   <!-- arm/o.le-v7-g/App        (plain ELF) -->
<configuration name="Device-Release"> <!-- arm/o.le-v7/App.so       (shared object!) -->
<configuration name="Device-Profile"> <!-- arm-p/o.le-v7-g/App -->
<configuration name="Simulator-Debug"><!-- x86/o-g/App -->
```
Because of the release `.so`, `main()` is written `Q_DECL_EXPORT int main(...)`.
For a rooted-device workflow you'll mostly use **Device-Debug** (plain ELF).

## Build & deploy (on the Windows build box; rooted device, no signing)

```bash
qmake CONFIG+=device                 # or CONFIG+=simulator; generates Makefiles from .pro
make Device-Debug                    # runs qcc + moc + rcc
blackberry-nativepackager -package MyApp.bar bar-descriptor.xml -devMode
blackberry-deploy -installApp -device <DEVICE_IP> -package MyApp.bar
```
Momentics does all four with one "Run" once the device IP+password are set. On a rooted
device the confirmed install path (per bbcord's README) is: **install the `.bar` with
[Sachesi](https://github.com/xsacha/sachesi) or DBBT** (rooting via https://bb10.root.sx).

## Linking libraries & third-party C/C++ code

- **System libs** via `.pro` `LIBS +=` / `QT +=`. bbcord uses:
  `QT += network sql` and `LIBS += -lbb -lbbdata -lbbmultimedia -lbbsystem -lsocket -lm -lz`.
  Rule of thumb: `-lbb*` = BlackBerry platform libs, `-lsocket` = QNX BSD sockets.
- **Third-party libs** (OpenSSL, websocketpp, asio, jsoncpp, mongoose, libwebsockets in
  bbcord): added as **git submodules** under `third_party/`, **cross-compiled to static
  `.a` per target** (device armle-v7 vs simulator x86), then linked with the full path,
  e.g. `LIBS += $$PWD/third_party/prebuilt/device/openssl/lib/libssl.a`. See
  `bbcord/scripts/build-openssl.sh` for the cross-compile pattern.
- Pass raw C flags to `qcc` with the `-Wc,` prefix, e.g. `QMAKE_CFLAGS += -Wc,-std=gnu99`.
- `.pro` convention: `APP_NAME = X` + `include(config.pri)`; `config.pri` is
  Momentics-generated and does source globbing + per-config `DESTDIR`. Don't hand-edit it.

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

- **[bbcord](https://github.com/michioxd/bbcord) and [SmartList10](https://github.com/BBerrylife/SmartList10)** — two real, complete Cascades
  apps. These are your primary references; read them before any web guide. See the two
  "reference" sections above for their architectures.
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
- Assuming Qt 5 / modern C++ — it's Qt 4.8 / GCC 4.6.3.
- Forgetting to declare a `<permission>` — the API just fails silently.
- Trying to compile on this Mac — build on the Windows box.
- Spending any effort on signing/debug tokens — the device is rooted; skip it.

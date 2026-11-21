---
name: bb10-cascades
description: Write BlackBerry 10 Cascades apps — Qt 4.8 with bb.cascades QML (up to 1.4, OS 10.3.1) and C++ built by the NDK's qcc. Use when creating, structuring or editing a BB10 app — main.cpp, exposing C++ to QML, Page/NavigationPane/TabbedPane/Sheet, TitleBar and ActionBar actions, ListView with data models, the application menu, Active Frames (SceneCover), invocation and share, clipboard, toasts and dialogs, settings and files, BPS events, Screen and ForeignWindowControl — or when checking whether a Cascades property or type exists. Covers the UTF-8/log/context-property boilerplate of main.cpp, page and scene swaps, C++-drawn list rows (references/writing-notes.md), the headless service entry point, TLS in Qt 4.8, and ForeignWindowControl z-order. C++ is GCC 4.6.3 (C++98) or 4.8.3 (gnu++11). Starts new apps from a template that builds. Pair with bb10-ux for look and feel, bb10-pitfalls before touching focus, keyboard, menus or native windows, and bb10-build-deploy to build and install.
---

# Cascades apps for BlackBerry 10

Cascades is BB10's native UI toolkit: QML (Qt 4.8's QtDeclarative, `import bb.cascades`) on top
of C++ (`bb::cascades::*`, Qt 4.8 core). Apps are built with the NDK's qcc in Docker (see **bb10-build-deploy**):
GCC 4.6.3 written as C++98 is the default; GCC 4.8.3 with `-std=gnu++11` is the other option proven on the phone
(HyperBerry, Tengram; [03](../../docs/03-toolchains.md) section 3.1). Reference projects (examples, not rules — prefer what the SDK and
the phone confirm): BlackBerry's Cascades samples, https://github.com/blackberry/Cascades-Samples,
indexed by topic in `references/samples.md`.

## Ground rules

1. **The API stops at 10.3.1.** The SDK (10.3.1.995) is the last one. Phones run 10.3.x.
   | Import | OS | Header tag | qmltypes `revision` |
   |---|---|---|---|
   | `bb.cascades 1.0` | 10.0 / 10.1 | `@since BlackBerry 10.0.0` / `10.1.0` | 0 |
   | `bb.cascades 1.2` | 10.2 | `@since BlackBerry 10.2.0` | 1, 2 |
   | `bb.cascades 1.3` | 10.3.0 | `@since BlackBerry 10.3.0` | 3 |
   | `bb.cascades 1.4` | 10.3.1 | `@since BlackBerry 10.3.1` | 4 |
   Also `bb.system 1.2` (SystemToast, SystemDialog, SystemPrompt, SystemListDialog,
   SystemProgressDialog, InvokeTarget, LocaleHandler...),
   `bb.data 1.0`, `bb.device 1.x`, `bb.platform 1.x`, `bb.multimedia 1.x`,
   `bb.cascades.pickers 1.0`. Write `import bb.cascades 1.4` unless older OS support is a goal.
2. **Verify an API before using it — never guess a property name.** Run
   `scripts/cascades-qmllint.sh` in bb10-build-deploy's folder (`base-bb10/skills/bb10-build-deploy/scripts/`)
   once; it caches the NDK's type info in `~/.cache/bb10-qmllint/imports`. Then:
   ```sh
   Q=~/.cache/bb10-qmllint/imports/bb/cascades/plugins.qmltypes
   grep -n -A40 'name: "bb::cascades::TextField"' $Q   # properties, signals, methods, exports
   grep -n '"bb.cascades/GridLayout' $Q                 # which import versions export a type
   ```
   (`revision: 259` there means 1.3, `260` means 1.4: the cache stores Qt 6's encoding.)
   C++ API and docs: headers in the NDK image,
   `docker run --rm yamsergey/bb10-ndk:0.6 grep -rn 'Q_INVOKABLE' /root/bbndk/target_10_3_1_995/qnx6/usr/include/bb/cascades/controls/page.h`.
   Every class has a forwarding header named after it (`bb/cascades/Page` includes
   `controls/page.h`; `bb/cascades/` lists them all).
   Groups: `bb/cascades/{controls,core,resources,layouts,...}`, `bb/system`, `bb/data`, `bb/device`,
   `bb/platform`, `bb/*.hpp` (libbb: `ImageData`, `AbstractBpsEventHandler`, `UIToolkitSupport`),
   `bps/`, `screen/`.
3. **A QML error quits the app at start with nothing on screen.** Run the checks of bb10-build-deploy ("Checks before
   every build": `lupdate` parse, handler check, `cascades-qmllint.sh assets/*.qml`) and write `QmlDocument::errors()`
   to a log, as the template's `main.cpp` does.
4. **No QtQuick visual items**: `Rectangle`, `Item`, `MouseArea`, `Row`, `Text`, `Image` don't
   render in a Cascades scene (the lint reports them as not found). `import QtQuick 1.0` is fine
   for its non-visual types: `QtObject` (logic objects), `Timer`, `Connections`, and the JS
   `openDatabaseSync()` storage API (one published app ships all four; not tried on the Q10). Our four apps use
   none of `Connections`; they `.connect()` in `onCreationCompleted` ([untested] on the phone, see `references/writing-notes.md`).
5. **Qt 4.8 and QML's JavaScript is ES5**: `var`, `function () {}`; no `let`/`const`, arrow
   functions, template strings, classes. C++ is GCC 4.6.3 written as C++98 by default (`SIGNAL()`/`SLOT()` connects,
   no lambdas, `auto`, `nullptr`, range-for); with GCC 4.8.3 and `-std=gnu++11` (HyperBerry, Tengram: verified on the
   Q10 and the Classic, 2026-11-20) `override`, `final` and the rest of C++11 are there, still on Dinkumware's library
   ([03](../../docs/03-toolchains.md)). No Qt 5 classes (no
   `QJsonDocument`: use `bb::data::JsonDataAccess`; no `QImage` when `QT -= gui`: use
   `bb::ImageData` + `bb::utility::ImageConverter`). Modern C++ goes in a separate process or
   C-interface library built with **bb10-cxx20**.

## Starting an app

Copy `template/` (a notes app laid out per the UI guidelines: NavigationPane, TitleBar,
Signature action with a key shortcut, a screen action in the ⋮ menu, list rows with
long-press context actions ending in Delete, Help in the application menu, SystemToast, a
registered `Timer`, Active Frame, QSettings, clipboard). It builds and packages with
`./build.sh` (checked in the NDK image; not yet run on a phone). Rename `hello`/`com.example.hello`
in `hello.pro`, `bar-descriptor.xml`, `build.sh`, the `qmlRegisterType` URI and the QML import.

`main.cpp` shape (from an earlier phone-verified app and the template):

```cpp
Application app(argc, argv);
Backend backend(&app);
qmlRegisterType<QTimer>("myapp", 1, 0, "Timer");   // bb.cascades has no Timer; before create()
QmlDocument *qml = QmlDocument::create("asset:///main.qml").parent(&app);
if (qml->hasErrors()) { /* log qml->errors() */ return 1; }
qml->setContextProperty("backend", &backend);
app.setScene(qml->createRootObject<AbstractPane>());
return Application::exec();
```

`.pro`: `TEMPLATE = app` (plain executable entry; the IDE's `cascades10` config instead makes a
`.so` for `<entryPointType>Qnx/Cascades</entryPointType>`), `QT += declarative network`,
`LIBS += -lbbcascades -lbbdata -lbbsystem -lbb -lbbutility` (+ `-lbps -lscreen` when used). The apps' own `main.cpp`
add UTF-8 codecs, a log that `qDebug` goes to, context properties on the root context and a parented root:
**read `references/writing-notes.md` before writing `main.cpp`** (and for page transitions, scene swaps, C++-drawn list rows).

**Timers**, two ways: Qt's `QTimer` registered as above (`singleShot`, `interval`, `onTimeout`,
`start()`; phone-verified), or `import QtQuick 1.0` and its `Timer` (`interval`,
`repeat`, `running`, `onTriggered`). Their property and signal names differ: don't
mix them up.

**Where the logic lives** — both work, pick per app:
- C++ controllers: one `QObject` controller per screen with `Q_PROPERTY` models and
  `Q_INVOKABLE` actions, all built and parented in one `ApplicationUI`, each a context property;
  controllers talk through signals wired in that one place. Suits networking and threads.
- QML logic: business logic in `QtObject` files instantiated in `attachedObjects`,
  one C++ object for what QML can't do. Suits UI-centric apps.
- Calling a QML function from C++: `QMetaObject::invokeMethod(root, "fnName", Qt::QueuedConnection)`.

## C++ ↔ QML

- Context properties (`setContextProperty`) for app objects; `Q_PROPERTY(... NOTIFY ...)` for
  anything QML binds to, `Q_INVOKABLE` for calls. `QVariantList`/`QVariantMap` arrive as JS
  arrays/objects (feed them to `ArrayDataModel.append(rows)`).
- QML can connect to C++ signals: `backend.message.connect(function (text) { ... })` in
  `onCreationCompleted`.
- Register Qt types QML lacks with `qmlRegisterType<T>("uri", 1, 0, "Name")` **before**
  `QmlDocument::create`.
- Pixels from C++ to an `ImageView`: `QVariant::fromValue(bb::cascades::Image(bb::ImageData))`
  as a property, bound to `image:` (no file round trip).

## Structure and navigation

- Root panes: `NavigationPane` (push/pop pages; `onPopTransitionEnded: page.destroy()`),
  `TabbedPane` (tabs), a bare `Page`. Modal tasks: `Sheet` with a `Page` whose `TitleBar` has
  `dismissAction` (left, Cancel/Close) and `acceptAction` (right, Save/Done).
- `ComponentDefinition` + `createObject()` for pages created on demand.
- Actions: `ActionItem` in `Page.actions` with `ActionBar.placement`: `OnBar`, `InOverflow` (the ⋮
  action menu), `Signature` (10.3, the highlighted centre action), `Default`. `DeleteActionItem`
  for destructive ones. `actionBarVisibility: ChromeVisibility.Hidden|Overlay|Compact|Visible`,
  `actionBarAutoHideBehavior` (`HideOnScroll`).
- `Page.openActionMenu()` (1.4) opens the overflow menu from code, even with the bar hidden.
  `ActionItem` has **no `visible`**: change the set with `addAction`/`removeAction` (details and
  traps in bb10-pitfalls).
- Long press: `contextActions: [ ActionSet { title: ...; ActionItem {...} } ]` on a control or
  list item. There is no API to open the context menu from code.
- Application menu (swipe down from the top): `Menu.definition: MenuDefinition { helpAction:
  HelpActionItem {}; settingsAction: SettingsActionItem {}; actions: [...] }`. An app without one
  must call `Application::setMenuEnabled(false)`, or the swipe opens an empty drawer.
- Keyboard shortcuts: `shortcuts: Shortcut { key: "A" }` on actions; `SystemShortcut` for the
  platform's (Space = page down, Shift+Space = up, T/B = top/bottom, ...). Disable letter
  shortcuts while a text field types.

## Lists

`ListView` + `ArrayDataModel` (flat, append JS objects), `GroupDataModel` (sorted, headers),
`XmlDataModel`, or a C++ `bb::cascades::DataModel`. Rows: `ListItemComponent { StandardListItem
{ title: ListItemData.title; description: ...; status: ...; imageSource: ... } }`. `onTriggered:
dataModel.data(indexPath)`. List items live in their own context: they cannot see ids outside;
call functions declared on the ListView through `id_of_item.ListItem.view.fn(...)`. Rebuild a
list only while it is visible (one app filled sheets on open; rebuilding on every change behind
the page cost UI-thread time during page loads).

## Layout

- `StackLayout` (`orientation`, children's `StackLayoutProperties { spaceQuota: 1 }`),
  `DockLayout` (overlay; alignment per child), `AbsoluteLayout`, `GridLayout` (1.3).
- Sizes in design units: `ui.du(n)`, `ui.sdu(n)` (rounded), `ui.px(n)` for true pixels (hairlines).
  Values per device: see **bb10-devices**.
- Margins space siblings only, never the container's edges: use padding for edges.
- Real size of a control: `attachedObjects: [ LayoutUpdateHandler { onLayoutFrameChanged:
  ... layoutFrame.width } ]`.
- Cascades animates property changes implicitly; turn it off per property when code drives it:
  `ImplicitAnimationController { propertyName: "translationY"; enabled: false }`.
- Per-device QML/images: asset selector folders (`assets/720x720/`, `assets/9ppd/`,
  `assets/mindw120h120du/`...), see bb10-devices.

## System integration

- **Active Frame**: `SceneCover` content shown while minimized (C++ `app.setCover()`, or QML
  `Application.setCover(cover)`; `MultiCover` 1.3 for sizes). **`cover` is a property of
  `Application`, not `Page`** (some third-party notes' `Page { cover: ... }` fails to load). A separate
  `cover.qml` document can't see `main.qml`'s ids: give it its own context properties. Q10 frame
  310×211 px, and the Q10 shows all of it above the system's title strip (measured 2026-10-31);
  it takes no touch input. Update it only between
  `thumbnail()` and `fullscreen()`, debounced (a 10 ms Timer).
- **Lifecycle**: signals of `bb::Application` (the base of `bb::cascades::Application`):
  `fullscreen()`, `thumbnail()`, `invisible()`, `asleep()`, `awake()`, `manualExit()`,
  `processStateChanged()`. They can fire twice in a row. BB10 may kill a backgrounded app with
  no clean exit: save state when it changes (`QSettings::sync()`).
- **Open/compose in other apps**: `InvokeRequest` + `InvokeManager::invoke()` — target
  `sys.browser` + `bb.action.OPEN` + uri for a link; `sys.pim.uib.email.hybridcomposer` +
  `bb.action.COMPOSE` + `mailto:` uri for email. Leave the target empty to
  let the system pick. Own share picker: `InvokeQueryTargetsRequest` → `InvokeQueryTargetsReply`
  (`actions()` → `targets()`).
- **Being invoked** (open links/files from other apps): `<invoke-target>` in the bar descriptor +
  `bb::system::InvokeManager::invoked(const bb::system::InvokeRequest&)` (also on cold start).
- **Share / invoke others**: `Invocation { query: InvokeQuery { mimeType: "text/plain";
  invokeActionId: "bb.action.SHARE" } }`; set `query.data`, `query.updateQuery()`, then
  `trigger("bb.action.SHARE")` in `onArmed`.
- **Clipboard**: `bb::system::Clipboard` — `clear()`, `insert("text/plain", utf8)`,
  `value("text/plain")`.
- **Messages**: `SystemToast` (`bb.system`; `autoUpdateEnabled: true` lets a new body replace one
  that is showing), `SystemDialog` (`onFinished: result == SystemUiResult.ConfirmButtonSelection`),
  `SystemPrompt`, `SystemListDialog`.
- **BPS events** in a Cascades app: subclass `bb::AbstractBpsEventHandler` next to `QObject`,
  `subscribe(virtualkeyboard_get_domain())` + `virtualkeyboard_request_events(0)`, handle in
  `event(bps_event_t*)`; link `-lbps`. Navigator swipe-down does not arrive this way: use
  `bb::UIToolkitSupport::swipedDown(bb::SwipeDownMode::Type)`.
- **Theme**: `<env var="CASCADES_THEME" value="dark"/>` (or `bright`, or
  `default?primaryColor=0xRRGGBB&amp;primaryBase=0xRRGGBB`) in the bar descriptor; switch at run
  time with `Application::instance()->themeSupport()->setVisualStyle(VisualStyle::Dark)` (10.3;
  wrap in `#if BBNDK_VERSION_AT_LEAST(10,3,0)` from `<bbndk.h>` only when building for older
  SDKs); a saved preference must be applied in `main()` before the scene is built. Colours from
  `ui.palette` (10.3).
- **Hub notifications**: `bb::platform::Notification` (`setTitle`, `setBody`,
  `setInvokeRequest`, `notify()`; `-lbbplatform`, `post_notification` permission).
- **Volume/media keys** (also while minimized): `bb::multimedia::MediaKeyWatcher` (`-lbbmultimedia`;
  one app found the mute/play key unreliable and added an `eventFilter` fallback).
- **Sensors**: QtMobility (`CONFIG += mobility`, `MOBILITY += sensors`), e.g. a
  `QAccelerometerFilter` subclass; debounce and add hysteresis or one tilt fires dozens of times.
- **App version / screen**: `bb::ApplicationInfo().version()` (from the bar descriptor);
  `bb::device::DisplayInfo().pixelSize()` (`-lbbdevice`).
- **Headless service** (an engine that outlives the UI; full story [09](../../docs/09-headless-service.md)):
  a second `Qnx/Elf` entry point with `<entryPointType>Qnx/Elf</entryPointType>`, system permissions `_sys_run_headless`,
  `_sys_headless_nostop` and **a memory class** (`_sys_headless_xlarge` = 30 MiB; **without one the heap cap is 3 MiB** and the
  engine dies before `main`), and an `application.headless` invoke target with a custom `<id>.START` action and
  `uris` `data://local`, which the UI invokes with an `InvokeRequest`. The engine must **reach the navigator within 6 s**
  or it is slain, and must ignore the navigator's own arguments. **A background app cannot invoke**: back off instead of
  retrying. **Start at boot** with a recurring invoke timer (`InvokeRecurrenceRule`, shortest interval 6 minutes) whose
  action is `bb.action.system.TIMER_FIRED` on the headless target; **not** `bb.action.system.STARTED`, which also fires
  after every install. [Q10, 2026-11-06/07] Measured instead of the old "2013 blog: ~3 MB, 7-25% CPU" figure: an idle Go
  engine held 6.4 MB of data for 30 min with no kill [Q10]; BBMusic's engine used 5.6% of a core playing 157 s [Q10] and
  7.7% over 9 min [Classic]; about 3% battery an hour for HyperBerry with the boot timer [Q10]. The `rlimit-patch` addon
  raises the cap to 128 MiB (root).

## Storage, files, network

- Sandbox: `QDir::homePath()` = writable `data/` (settings, databases, caches);
  `QCoreApplication::applicationDirPath()` = read-only `app/native` (packaged assets, `asset:///`).
  Shared storage `/accounts/1000/shared/{misc,downloads,documents,photos,...}` needs
  `access_shared`, which the user can refuse: fall back to `data/`.
- Settings: `QSettings(QDir::homePath() + "/settings.ini", QSettings::IniFormat)`. JSON:
  `bb::data::JsonDataAccess`. SQL: QtSql + SQLite (`quotes` sample).
- **TLS (Qt 4.8's QtNetwork).** "Qt 4.8 speaks TLS 1.0 only" was wrong and was corrected on 2026-11-20 (`hyperberry` pitfalls). Read off
  the Classic (SQC100-1, OS 10.3.3.3216): the phone's `/usr/lib/libssl.so.2` is **OpenSSL 1.0.2g** (TLS 1.2, ECDHE-GCM suites), and
  `libQtNetwork.so.4.8.6` uses `SSLv23_client_method`, which takes the highest version both ends have; only an explicit
  `QSsl::TlsV1` pins 1.0. [Classic] **Not run:** a handshake from a Qt app (log `sessionCipher().protocolString()` to find out).
  Still true: no TLS 1.3, **a CA store from 2018**, no redirects followed. Plain HTTP works. The built-in `WebView` is a 2014 WebKit
  with an old certificate store [Q10]. HyperBerry keeps the network in the engine by choice (Go's TLS 1.3 and current roots, tests on the
  PC, one writer of the database; BBMusic and Phoenix ship their own TLS and CA bundle), not because the UI cannot: log a handshake
  before you rule QtNetwork out. An older [Q10] note saw modern HTTPS fail with "SSL handshake failed"; its stated cause is what was corrected. In-process C libraries with their own TLS are possible
  (a static OpenSSL built with the NDK's GCC, as one third-party app does; it skipped certificate checks) but the apps did not.
  [06](../../docs/06-third-party-libs.md) has the libraries.
- **Threads**: blocking work on a `QThread` with a worker `moveToThread()`; results back by
  queued signals. **Change `DataModel`s and Cascades objects only on the UI thread.**
- `QTextStream`: call `setCodec("UTF-8")` for text files.
- 64-bit ids cross into QML as **strings** (JS numbers lose precision above 2^53).

## Native windows (ForeignWindowControl)

Embedding another Screen window (game, video, a browser engine): the child window must be a `SCREEN_CHILD_WINDOW`, join
`Application::instance()->mainWindow()->groupId()`, and carry the control's `windowId` as `SCREEN_PROPERTY_ID_STRING`
(set before the group join). Two z-order arrangements exist, with opposite trade-offs:

| Arrangement | What the phone does | Shipped by |
|---|---|---|
| **Above Cascades** | the window draws above all Cascades content **and takes touches**; nothing Cascades draws (toast, action menu, text magnifier, overlays) can appear over it. A camera viewfinder window is always this | a browser-engine window [Q10]; the camera viewfinder |
| **z -1, under a transparent hole** (the SDK's way) | Cascades controls draw over the video and their taps work; the window itself **gets no touches** (Cascades forwards only keys) | **HyperBerry's and Tengram's `VideoWindow`** ([12](../../docs/12-video-and-audio.md) section 5: "+1 also tried; under the hole is the one kept"); the system player's window |

The older sentence "draws above all Cascades content and takes touches" described only the first case. Pick the second when the controls
over the picture matter and the picture needs no touches; read the ForeignWindowControl section of **bb10-pitfalls** (clip, snapshot, focus)
before designing around the first.

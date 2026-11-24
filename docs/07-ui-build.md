# Building the Cascades UI

Read this when: you write the `.pro` file, the build script or `main.cpp` of a new app's UI, or the app quits at start / a page never opens and you need to know which check would have caught it.

Code referenced here is in [../samples/](../samples/). The UI is built in `yamsergey/bb10-ndk:0.6` (images and compiler choice: [03-toolchains.md](03-toolchains.md)); the descriptor and the `.bar` are in [08-packaging.md](08-packaging.md). Cascades coding rules and UI behaviour traps are in the skills, not here: [../skills/bb10-cascades/SKILL.md](../skills/bb10-cascades/SKILL.md), [../skills/bb10-pitfalls/SKILL.md](../skills/bb10-pitfalls/SKILL.md).

Source priority: where `.pro` files, build scripts and 2026-11 notes disagree with an app's own older notes (for example hyperberry's notes), the newer app source wins. Cases are listed under "Where sources disagree" at the end.

## 1. The `.pro` file of a plain-executable app

The UI is a plain executable, the bar's `Qnx/Elf` entry point. That is the simplest build, and it costs about 3 s at every start: [14-cascades-launcher.md](14-cascades-launcher.md) has the three changes that start the same UI through BB10's Cascades launcher instead (Tengram, verified on the Classic 2026-11-24; HyperBerry builds the same way, not yet tried on a phone). Minimal template: [samples/ui/app.pro](../samples/ui/app.pro); bbmusic's [`bbmusic.pro`](../samples/ui/bbmusic.pro) is the same with `network`.

```
TEMPLATE = app
TARGET = phoenix
CONFIG += qt warn_on release
QT += declarative
LIBS += -lbbcascades -lbbdata -lbbsystem -lbb -lbps
QMAKE_CXXFLAGS += -Wno-psabi

SOURCES += src/main.cpp src/EngineLink.cpp src/Thumbs.cpp src/Prefs.cpp src/Opener.cpp src/Session.cpp
HEADERS += src/EngineLink.h src/UiLog.h src/Thumbs.h src/Prefs.h src/Opener.h src/Session.h src/Router.h

DESTDIR = app
OBJECTS_DIR = obj
MOC_DIR = moc

TRANSLATIONS += translations/phoenix_it.ts translations/phoenix_fr.ts translations/phoenix_de.ts \
    translations/phoenix_pl.ts translations/phoenix_ru.ts translations/phoenix_hi.ts \
    translations/phoenix_vi.ts translations/phoenix_id.ts
```

([samples/ui/app.pro](../samples/ui/app.pro), comments dropped.)

| Line | Why |
|---|---|
| `TEMPLATE = app` | plain executable, not a `.so` (section 2) |
| `CONFIG += qt warn_on release` | no `cascades10` |
| `QT += declarative` | QML (Qt 4.8 QtDeclarative). bbmusic adds `network` (bbmusic's [`bbmusic.pro`](../samples/ui/bbmusic.pro)) |
| `LIBS += -lbbcascades -lbbdata -lbbsystem -lbb -lbps` | Cascades; `bb::data` (JSON, `JsonDataAccess`); `bb::system` (`InvokeManager`, `LocaleHandler`); `bb` core; BPS (phoenix's [`EngineLink.cpp`](../samples/engine-link/stdio/EngineLink.cpp) and the other apps' sources use these classes) |
| `QMAKE_CXXFLAGS += -Wno-psabi` | in every one of the four `.pro` files; no source says why. Keep it |
| `DESTDIR = app` | the executable lands in `<build dir>/app/<TARGET>`; the descriptor points at it (`../build/app-ui/app/phoenix`, [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml)) |
| `OBJECTS_DIR`, `MOC_DIR` | keeps `obj/` and `moc/` inside the build dir |
| `TRANSLATIONS` | the `.ts` files `lupdate` fills and `lrelease` compiles (section 6) |

The compiler is the NDK default (gcc 4.6.3) in phoenix and bbmusic. hyperberry and tengram override it with gcc 4.8.3 and `-std=gnu++11`; the exact qmake lines and why are in [03-toolchains.md](03-toolchains.md) section 3.1. Do not repeat them here.

### Additions the bigger apps make (hyperberry's [`hyperberry.pro`](../samples/ui/hyperberry.pro), [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro))

| Addition | What it is for (source) |
|---|---|
| `QT += declarative network sql gui` | `gui`: "QtGui links anyway (QtDeclarative needs it); QImage and QImageReader make the photo previews and the thumbnail sent with a photo" (comment in both files). `sql`: the UI reads `chats.db` itself (hyperberry's notes section 3) |
| `-lbbdevice` | `bb::device::DisplayInfo` (hyperberry's `src`) |
| `-lbbplatform` | `bb::platform::NotificationDefaultApplicationSettings` (hyperberry's `src`) |
| `-lbbutilityi18n` | `bb::utility::i18n::Formatting` and `RelativeDateFormatter` (dates) |
| `-lslog2` | `sys/slog2.h`: the UI's log buffer in [samples/ui/main-logging.cpp](../samples/ui/main-logging.cpp) |
| `-lbbpim` | `bb::pim::contacts` (address book, [`ContactBook.cpp`](../samples/cookbook/ContactBook.cpp)) |
| `-lasound -laudio_manager` | call audio in the UI (`CallAudio.cpp`: `asoundlib` and `audio/audio_manager_*.h`) |
| `-lscreen -lcamapi` | `screen/screen.h` and `camera/camera_api.h`, `camera/camera_encoder.h` are included in the UI sources (video window and camera encoder files) |
| `-lsocket` (tengram only) | with the static `libcrypto.a`, which needs it ([samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)) |
| `-L$$PWD/../build/prefix/lib -ltgs -lz` (tengram) | `libtgs.so`, an rlottie build by clang that exports only a C API; the UI calls it as plain C. It ships in the bar as `lib/libtgs.so` ([08-packaging.md](08-packaging.md)) |
| `QMAKE_LFLAGS += -Wl,-rpath-link,$${QNX_DIR}/$${QNX_CPUDIR}/usr/lib/qt4/lib` | present in hyperberry, tengram and [skills/bb10-cascades/template/hello.pro](../skills/bb10-cascades/template/hello.pro); phoenix and bbmusic link without it and no source says why. Copy it when you add libraries beyond the phoenix set |
| `QMAKE_LFLAGS += -Wl,--gc-sections` and `-Wl,--exclude-libs,ALL` (tengram) | "Nothing of the static libcrypto may show in the executable's dynamic symbols: a system library that uses the phone's own OpenSSL 1.0 (QtNetwork, say) would bind to ours, 3.5, and crash." Background: [06-third-party-libs.md](06-third-party-libs.md) |

Other `-lbb*` modules exist; the skill names `-lbbmultimedia` for `MediaKeyWatcher` and `-lbbutility` for `bb::utility` ([skills/bb10-cascades/SKILL.md](../skills/bb10-cascades/SKILL.md)).

### C sources compiled inside the UI

hyperberry and tengram compile third-party C files straight into the UI target. What the `.pro` files do:

- `$$PWD/../lib/qrcodegen/qrcodegen.c` with `QMAKE_CFLAGS += -std=c99` (hyperberry) or `-std=gnu99` (tengram). Tengram's comment: "gnu99 and not c99, which is strict ISO C and hides the POSIX functions (open, fcntl, pthread_mutexattr_settype) that SQLite's amalgamation calls: it does not compile."
- libwebp 1.4.0, decoder only, listed by `for(f, WEBP_DEC): SOURCES += $$WEBP_DIR/src/dec/$${f}.c` loops. "qcc passes -mcpu=cortex-a9 and no -mfpu=neon, so __ARM_NEON__ is unset and libwebp's NEON, SSE and MIPS files compile to empty stubs: the plain C code decodes." A `ponytail:` comment records that `-mfpu=neon` is untried.
- SQLite amalgamation and Tengram's SQLCipher driver (`SQLITE_HAS_CODEC` and the other `DEFINES` copied from TDLib's flags), `QMAKE_CFLAGS_WARN_ON = -w` to silence the C libraries' warnings.
- `$$files($$PWD/src/*Bubble.cpp)`: a glob, so files added in parallel need no `.pro` edit. `$$PWD`-relative because qmake runs in the build folder; re-run qmake after adding a file (hyperberry's notes, "Building and installing").
- The OpenMAX IL headers are fetched and put on `INCLUDEPATH`; the core library itself is `dlopen()`ed from the phone (`/base/lib/libOmxCore.so`), so nothing links.
- C defines do not go through `DEFINES` alone: "`DEFINES` reach C++ only, so C defines go through `-Wc,"-D..."` too" ([skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md)). Tengram's [`.pro`](../samples/ui/app-gcc483.pro) sets `DEFINES` for `sqlite3.c`; check that your C file really sees them.

### lupdate and codecs

```
# lupdate reads the QML too; scripts/build-app.sh turns the .ts into the .qm the bar ships.
CODECFORTR = UTF-8
CODECFORSRC = UTF-8
lupdate_only {
    SOURCES += assets/*.qml assets/errors.js
}
```

(hyperberry's [`hyperberry.pro`](../samples/ui/hyperberry.pro); the same block is in [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro).) `lupdate_only` makes `lupdate` read the QML and JS; `make` never sees them. `CODECFORTR` and `CODECFORSRC` tell `lupdate` the strings are UTF-8, matching the codecs set in `main.cpp` (section 5).

## 2. Why not `CONFIG += cascades10`

`CONFIG += cascades10` is the Momentics default (and a third-party starter app's). It builds a release as `o.le-v7/lib<name>.so`, for `<entryPointType>Qnx/Cascades</entryPointType>`; the descriptor must then point at that `.so` and `main` needs `Q_DECL_EXPORT`. "A descriptor that expects an executable fails with `Invalid asset path` (checked by building that starter app)." Without `cascades10` (`TEMPLATE = app` plus the libs) the output is a plain executable wherever `DESTDIR` says ([skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md)).

So: no `cascades10`, no Momentics `config.pri` (a generated file with source globbing and per-config `DESTDIR`; "a self-contained `.pro` builds fine with plain qmake"), and `Q_DECL_EXPORT int main(int argc, char **argv)` as in [samples/ui/main.cpp](../samples/ui/main.cpp). The template comment says the same ([skills/bb10-cascades/template/hello.pro](../skills/bb10-cascades/template/hello.pro)): either works, the plain executable is simpler.

"Either works" was written before the start was measured. The `Qnx/Cascades` entry point needs neither `cascades10` nor `config.pri` (`TEMPLATE = lib` and `CONFIG += plugin` in the same self-contained `.pro`), and it brought Tengram's `main()` from 1.5 s after the tap to 0.15 s and its `Application` from 3.4 s to 0.3 s: [14-cascades-launcher.md](14-cascades-launcher.md).

## 3. The build command

Verbatim shape from [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh) (out-of-tree, in the NDK image):

```sh
docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" bash -c '
	source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
	set -eu
	mkdir -p build/app-ui
	...
	(cd build/app-ui && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../../app/phoenix.pro >/dev/null &&
		make -j"$(nproc)" >/dev/null)
	ls -l build/app-ui/app/phoenix
	cd app && blackberry-nativepackager -package ../build/phoenix.bar \
		-buildIdFile ../build/app-buildid.txt bar-descriptor.xml -configuration Device-Release
	rm -f assets/.assets.index'
```

with `IMAGE=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}`. Compile only, without packaging (no build id spent): the same `qmake` and `make` lines, about 30 s when little changed; two builds at once need two build folders (hyperberry's notes).

## 4. The checks that run before every build

Why they exist: a QML error makes the app quit at start with nothing on screen, or a page never opens. Neither the build nor the phone says why. Each check below came from an incident.

| # | Check | Catches | Incident |
|---|---|---|---|
| 1 | `lupdate app/assets/*.qml -ts /tmp/qml-check.ts 2>&1 \| grep -A2 "error:"` | syntax Qt 4.8 cannot parse. `lupdate` is the NDK's, so it parses as the phone does. The Qt 6 lint lets some syntax through | `Recorder.qml`, hyperberry 0.0.1.77: a binding written as a block with a `var` in it (`clock: { var s = ...; return ... }`) passed the lint and failed the chat page (hyperberry's [`build-app.sh`](../samples/ui/scripts/build-app-hyperberry.sh); hyperberry's notes) |
| 2 | `grep -n "readonly property" app/assets/*.qml` | `lupdate` accepts `readonly property`, the phone refuses the page ("Readonly not yet supported") | the call screen never opened in hyperberry 0.3.1.371-373 (hyperberry's [`build-app.sh`](../samples/ui/scripts/build-app-hyperberry.sh)) |
| 3 | the sed/uniq check for a function defined twice on a page's root object | the page parses, then the phone refuses it ("Duplicate method name"); `lupdate` does not see it | tengram 0.1.0.29, `ChatPage`'s `picked` ([samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh)) |
| 4 | `python3 scripts/check-qml-handlers.py` | a handler for a signal that does not exist ("Cannot assign to non-existent property"): the app quits at start | `cascades-qmllint.sh` does not look at handler names (tried 2026-11-14, per the script's docstring) |
| 5 | `cascades-qmllint.sh app/assets/*.qml` | misspelt properties, signals and enum values; wrong child types; duplicate ids; syntax | the base lint; see below |
| 6 | the `data/check-qml` flag file, at run time | every QML document compiled on the phone, results in the UI log | pages are created only when first opened, so a fault shows only as a page that never appears |
| 7 | `node app/tests/*.js` | the app's pure JS (formatting, queue arithmetic) | phoenix's [`format_test.js`](../samples/ui/tests/format_test.js), bbmusic's [`queue_test.js`](../samples/ui/tests/queue_test.js), hyperberry's [`test-phone.js`](../samples/ui/tests/test-phone.js) |

### 4.1 Check 3, as written in [samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh)

```sh
dups=$(for f in "$ROOT"/app/assets/*.qml; do
	sed -n 's/^    function \([A-Za-z0-9_]*\)(.*/\1/p' "$f" | sort | uniq -d | sed "s|^|$(basename "$f"): |"
done)
if [ -n "$dups" ]; then
	echo "build-ui.sh: a function defined twice on a page's root object:" >&2
	echo "$dups" >&2
	exit 1
fi
```

It matches only `function` lines indented by exactly four spaces (the root object's own members, in the layout these files use). Keep that indentation.

### 4.2 Check 4: `scripts/check-qml-handlers.py`

Source: phoenix's `check-qml-handlers.py` (bbmusic has its own copy; the two differ only in a comment on the `EXTRA` table).

- How it works: it checks every `onSomething:` handler (`onTriggered:`, `onMoveEnded:`, `input.onSubmitted:`) against the SDK's type information: the signal must exist on the element the handler sits in. It reads QML by its layout (one element or property per line, as the files are written), with a brace stack, not a parser. A line it cannot place is reported rather than passed: it counts the handlers written and the handlers placed, and fails on a mismatch. Handlers `onXChanged` for a property declared in QML pass. A custom page type in `app/assets/<Name>.qml` is resolved through its root element.
- What it needs: the type cache `~/.cache/bb10-qmllint/imports` that `cascades-qmllint.sh` makes on its first run (`plugins.qmltypes` of the NDK's `bb.*` modules, converted). Without the cache it exits with "run cascades-qmllint.sh once". [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh) runs it only when that folder exists and otherwise prints "no qmllint cache, signal handlers not checked".
- What you edit per app: the `EXTRA` dict at the top, which lists types registered from C++ (`Timer` from `qmlRegisterType<QTimer>`: signal `timeout`, props `interval`, `singleShot`). Add your own registered types there or they are reported as unknown.
- Run it: `python3 scripts/check-qml-handlers.py` (exit 1 if anything is wrong); `--self-test` proves it still finds three kinds of bad handler.
- It assumes the app lives at `<root>/app/assets/*.qml` and the script at `<root>/scripts/`.
- A shared copy lives in this repo, [skills/bb10-build-deploy/scripts/check-qml-handlers.py](../skills/bb10-build-deploy/scripts/check-qml-handlers.py). It takes the project root as an argument (`check-qml-handlers.py ../myapp`, default the current directory) and gives the same result as Phoenix's and BBMusic's own copies on those two apps (run 2026-11-21). Copy it into a new app's `scripts/` when the app registers types of its own.

### 4.3 Check 5: `cascades-qmllint.sh`

In base-bb10: [skills/bb10-build-deploy/scripts/cascades-qmllint.sh](../skills/bb10-build-deploy/scripts/cascades-qmllint.sh) (with `qmltypes6.py` and `qtquick1.qmltypes`; the same script is used by the app repos' build scripts). Qt 6 `qmllint` with the NDK's own Cascades type info, cached in `~/.cache/bb10-qmllint` (first run pulls the type info from the NDK image; on macOS it runs qmllint in a small Debian image).

- **Exit 255** = syntax error or duplicate id: the app would quit at start. Stop the build.
- **Warnings (exit 0) are real**: misspelt properties, signals or enum values ("Did you mean `hintText`?"), wrong child types (an `ActionItem` in a `Container`), two children in a `Page`'s single `content`. Read every one.
- `import QtQuick 1.0`: `Timer`, `Connections`, `QtObject` are known; visual QtQuick items (`Rectangle`, `MouseArea`) are reported as not found, which is right: they do not render in Cascades.
- Known false positives: types registered from C++ (`Timer`, its import "not found"); functions declared on a `ListView` and called as `item.ListItem.view.fn()`; custom properties read through an id typed as a base type (`page.lid` on a `Page`); `layoutProperties.positionX`; Qt 4 globals (`Qt.formatDateTime`, `Qt.openUrlExternally`). Context properties are not checked.
- It does not check that an API exists in an older import version; 1.4 is the newest.
- Phoenix's script calls it, but with a path to hyperberry's copy hard-coded in `LINT=`; a new app should point at its own copy of the base-bb10 script.

### 4.4 Check 6: compile every document on the phone

[samples/ui/main.cpp](../samples/ui/main.cpp) and bbmusic's `main.cpp` carry the same `checkDocuments(Application &app)`. If the flag file `data/check-qml` exists (put there by hand over ssh; `QDir::homePath() + "/check-qml"`), every `*.qml` under `app/native/assets` is created with `QmlDocument::create("asset:///" + name)` at start and the result goes to the UI log: `check: ok X.qml`, `check: FAIL X.qml: <error>`, then `check: N of M documents failed`. It finds what the compiler finds (unknown types, properties, signal handlers, enum values, JavaScript that does not parse), not what a binding does when it runs. It needs the running app: "QmlDocument aborts without a bb::cascades::Application, and that cannot be built from a shell (both tried, 2026-11-14)". bbmusic skips it when started as the sign-in card. Result on record: bbmusic 0.1.0.19, 2026-11-14, "all 16 documents compiled on the phone (the `check:` lines)" (bbmusic's notes).

Always log `QmlDocument::errors()` for the root document as well (section 5): hyperberry writes them to `hyperberry-qml-errors.log` in `/accounts/1000/shared/misc` or `data/` ([samples/ui/main-logging.cpp](../samples/ui/main-logging.cpp)); phoenix and bbmusic write `QML: ...` lines to the UI log and `return 1`.

### 4.5 Ready-to-copy block for a new app's build script

Assembled from the four scripts; adapt `NAME`, paths and the language list. Comments marked "added" are not in any source script.

```sh
#!/bin/sh
# Builds build/NAME.bar: the UI in the Native SDK image, packaged unsigned. Each package gets the next build id.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=${BB10_NDK_IMAGE:-yamsergey/bb10-ndk:0.6}
LINT=${LINT:-$ROOT/skills/bb10-build-deploy/scripts/cascades-qmllint.sh}
mkdir -p "$ROOT/build"

# 3. A function defined twice on a page's root object (scripts/build-ui.sh).
dups=$(for f in "$ROOT"/app/assets/*.qml; do
	sed -n 's/^    function \([A-Za-z0-9_]*\)(.*/\1/p' "$f" | sort | uniq -d | sed "s|^|$(basename "$f"): |"
done)
if [ -n "$dups" ]; then
	echo "build-app.sh: a function defined twice on a page's root object:" >&2
	echo "$dups" >&2
	exit 1
fi
# 4. A signal handler that does not exist (needs the cache the lint makes; scripts/build-app.sh).
if [ -d "$HOME/.cache/bb10-qmllint/imports" ]; then
	python3 "$ROOT/scripts/check-qml-handlers.py"
else
	echo "build-app.sh: no qmllint cache, signal handlers not checked" >&2
fi
# 7. The app's pure JS (scripts/build-app.sh runs one file; this loops, added).
if command -v node >/dev/null 2>&1; then for t in "$ROOT"/app/tests/*.js; do node "$t"; done; fi
# 5. Exit 255 stops the build; read the warnings.
if [ -f "$LINT" ]; then
	(cd "$ROOT" && sh "$LINT" app/assets/*.qml)
else
	echo "build-app.sh: $LINT not found, QML not linted" >&2
fi

[ -f "$ROOT/build/app-buildid.txt" ] || echo 1 >"$ROOT/build/app-buildid.txt"
docker run --rm -v "$ROOT:/src" -w /src "$IMAGE" bash -c '
	source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
	set -eu
	mkdir -p build/app-ui
	# 1. lupdate parses QML with Qt 4.8, as the phone does.
	if lupdate app/assets/*.qml app/assets/*.js -ts /tmp/qml-check.ts 2>&1 | grep -A2 "error:"; then
		echo "build-app.sh: QML that Qt 4.8 cannot parse" >&2
		exit 1
	fi
	# 2. lupdate takes readonly property, the phone refuses the page.
	if grep -n "readonly property" app/assets/*.qml; then
		echo "build-app.sh: readonly properties fail on the phone" >&2
		exit 1
	fi
	# Added: no Component.onCompleted (hyperberry pitfalls: neither lint nor lupdate catches it).
	if grep -n "Component\.on" app/assets/*.qml; then
		echo "build-app.sh: Component.onCompleted needs import QtQuick 1.0; use onCreationCompleted" >&2
		exit 1
	fi
	mkdir -p app/translations build/qm
	lupdate -silent -no-obsolete app/assets/*.qml app/assets/*.js app/src/*.cpp \
		-ts app/translations/NAME_{it,fr,de,pl,ru,hi,vi,id}.ts
	for ts in app/translations/*.ts; do
		lrelease -silent "$ts" -qm "build/qm/$(basename "${ts%.ts}").qm"
	done
	(cd build/app-ui && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../../app/NAME.pro >/dev/null &&
		make -j"$(nproc)" >/dev/null)
	ls -l build/app-ui/app/NAME
	cd app && blackberry-nativepackager -package ../build/NAME.bar \
		-buildIdFile ../build/app-buildid.txt bar-descriptor.xml -configuration Device-Release
	rm -f assets/.assets.index'
unzip -p "$ROOT/build/NAME.bar" META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'
unzip -l "$ROOT/build/NAME.bar"
```

Notes on the block: the single-quoted string must contain no apostrophe (phoenix's comment says so). Chain build steps with `&&`: under `;` an early failure went unnoticed (hyperberry's notes). The `Component\.on` grep is a simple pattern; `Component.onCompleted` is legal if the page has `import QtQuick 1.0` (hyperberry's notes), so drop the grep if you use that import.

## 5. `main.cpp` boilerplate the apps share

From [samples/ui/main.cpp](../samples/ui/main.cpp):

```cpp
Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    // tr() and C strings read as UTF-8 (Qt 4 takes them as Latin-1).
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    uiLog("ui started");
    ...
    QTranslator translator;
    if (translator.load("phoenix_" + QLocale().name(), "app/native/qm"))
        app.installTranslator(&translator);
    ...
    // bb.cascades has no Timer.
    qmlRegisterType<QTimer>("phoenix", 1, 0, "Timer");
    QmlDocument *document = QmlDocument::create("asset:///main.qml").parent(&app);
    if (document->hasErrors()) {
        // A QML error otherwise quits the app at start with nothing to show.
        foreach (const QDeclarativeError &error, document->errors())
            uiLog("QML: " + error.toString());
        return 1;
    }
    ...
    AbstractPane *root = document->createRootObject<AbstractPane>();
    if (!root) {
        uiLog("QML: no root object");
        return 1;
    }
    root->setParent(document);
    app.setScene(root);
    checkDocuments(app);
    engine.start();
    const int rc = Application::exec();
    app.setScene(0);
    return rc;
}
```

- **Codecs.** tengram's comment on the same two lines: "typing..." showed as "typingà|" (Q10, 2026-10-31). bbmusic's `main.cpp` does not set them; a new app should (the Latin-1 default is what broke tengram's text). `QTextStream` also needs `setCodec("UTF-8")` for non-ASCII (hyperberry's notes).
- **Translator.** phoenix and bbmusic: `QTranslator::load("<app>_" + QLocale().name(), "app/native/qm")`, once at start ("a change in the system's settings shows on the next launch": `ponytail:` comment in both). tengram and hyperberry: the language part of `bb::system::LocaleHandler(bb::system::LocaleType::Messages).locale().name()`, `load("tengram_" + language, QCoreApplication::applicationDirPath() + "/assets/translations")`, set before the QML loads, whose `qsTr` reads it (tengram's `main.cpp`).
- **Timer.** `qmlRegisterType<QTimer>("<app>", 1, 0, "Timer")` then `import <app> 1.0`. Add it to `EXTRA` in `check-qml-handlers.py` (4.2).
- **The UI log.** [samples/ui/UiLog.h](../samples/ui/UiLog.h): `uiLog(const QString&)` appends `yyyy-MM-dd hh:mm:ss.zzz text` to `ui.log` in `QDir::homePath()`; a file past 512 kB is removed. bbmusic's `bbmusic-ui.log` has no cap and "never logs the engine's token". hyperberry and tengram route `qDebug`/`qWarning` to the system log (`slog2info -b <app>`) and to `data/logs/ui.log` with a handler installed by `qInstallMsgHandler` ([samples/ui/main-logging.cpp](../samples/ui/main-logging.cpp)): "qDebug and qWarning, QML's errors included, go nowhere on the phone by themselves". More in [11-testing-and-debugging.md](11-testing-and-debugging.md).
- **Context properties on the root context.** phoenix's comment: set them with `QmlDocument::defaultDeclarativeEngine()->rootContext()->setContextProperty(...)`, "not the document's: a list item is made in a context of its own, which the document's context properties do not reach. On the phone (0.1.0.14, 2026-11-21) every row's `prefs.textScale` came out as nothing". bbmusic sets them on the document (`document->setContextProperty`); use the root context if a list's rows read them.
- **Scene ownership.** bbmusic: "`setScene()` deletes a replaced scene that the Application owns, and it owns one that came without a parent: so the root gets one" (`root->setParent(document)`), and `app.setScene(0)` before returning.
- **Start the engine last**, after the scene is set (`engine.start()`), and start order for the service: [09-headless-service.md](09-headless-service.md).

## 6. QML and JavaScript limits the checks exist for

Qt 4.8 QML, `bb.cascades 1.4`. Build- and parse-level facts from hyperberry's notes ("Starting up and QML", "Qt 4.8 and C++") and bbmusic's notes ("Read before writing QML"):

| Limit | Consequence |
|---|---|
| JavaScript is ES5 | `var` and `function`; no `let`, arrows or template strings. Brace every multi-line `if` |
| The lint parses with Qt 6, the phone with Qt 4.8 | a `var` inside a binding block fails on the phone: use a function. Check 1 |
| No QtQuick visual items (`Rectangle`, `Item`, `MouseArea`, `Text`, `Image`) | they do not render in a Cascades scene. `QtObject`, `Timer`, `Connections` are fine |
| Cascades has no `Timer` | register `QTimer`, or `import QtQuick 1.0` (its `Timer` uses `repeat`, `running`, `onTriggered`; the registered one uses `singleShot`, `interval`, `onTimeout`). Mixing the names silently does nothing |
| No `Component.onCompleted`/`onDestruction` without `import QtQuick 1.0` | the document fails ("Non-existent attached object"); neither lint nor `lupdate` catches it (hyperberry 0.0.1.179). Use `onCreationCompleted` |
| `bb.system` types (`SystemDialog`, `SystemToast`) | need `import bb.system`; they have no `onCreationCompleted`. Either mistake fails the whole document, and for the root page a black screen |
| `cover` belongs to `Application`, not `Page` | `Page { cover: ... }` fails to load |
| `lupdate` cannot parse a JS file with `.pragma library` | [`errors.js`](../samples/ui/errors.js) dropped it to be translated; it holds no state |
| Ids as numbers | JS numbers lose precision past 2^53: ids are strings across the socket and into QML |
| QML numbers reach C++ as doubles | `JsonDataAccess` does not write them as plain integers, which Go's integer fields refuse (`EngineLink::send` turns whole numbers into integers) |
| `Q_INVOKABLE` with a default argument | moc clones it per arity; prefer two names (untested on BB10) |
| `QElapsedTimer` has no constructor | call `invalidate()` in the constructor: stack garbage read as "a START went out just now" and the engine never started |
| A forward-declared class does not convert to its base | include the header |
| C++ dialect | gcc 4.6.3 means C++98 (no `auto`, lambdas, `nullptr`, range-for; `SIGNAL`/`SLOT`); hyperberry and tengram compile with gnu++11 ([03-toolchains.md](03-toolchains.md)); phoenix and bbmusic stay on the default |

Behaviour-level traps (focus, keyboard, menus, lists, `ListItem.view`, a changed-signal that also fires when code sets the value) are in [../skills/bb10-pitfalls/SKILL.md](../skills/bb10-pitfalls/SKILL.md). The Mac editor's red C++ errors for `bb/cascades/...` includes are noise: the NDK's compile is the check (hyperberry's notes).

## 7. Translations

| Step | Command and file | Source |
|---|---|---|
| Update `.ts` | phoenix: `lupdate -silent -no-obsolete app/assets/*.qml app/assets/*.js app/src/*.cpp -ts app/translations/phoenix_{it,fr,de,pl,ru,hi,vi,id}.ts` | [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh) |
| English file | bbmusic keeps `bbmusic_en.ts` for plurals only: `lupdate -silent -pluralonly ... -ts app/translations/bbmusic_en.ts`; the other files are updated with `$(ls app/translations/*.ts \| grep -v _en.ts)` | bbmusic's [`build-app.sh`](../samples/ui/scripts/build-app-bbmusic.sh) |
| Compile | `lrelease -silent "$ts" -qm "build/qm/$(basename "${ts%.ts}").qm"` | both |
| Ship | phoenix and bbmusic: `<asset path="../build/qm">qm</asset>`, loaded from `app/native/qm` | [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml) |
| Ship (hyperberry, tengram) | `lrelease "$ts" -qm "app/assets/translations/$(basename "$ts" .ts).qm"`; they ride inside the `assets` asset and load from `applicationDirPath() + "/assets/translations"` | hyperberry's [`build-app.sh`](../samples/ui/scripts/build-app-hyperberry.sh), [samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh) |

- The sources are English, "which has no file" ([samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh)). bbmusic's English `.ts` exists only to pluralise ("1 track", "5 tracks").
- **Default set for a new app: en (source), it, fr, de, pl, ru, hi, vi, id** (the user's standing instruction). Existing apps keep their own lists: phoenix it fr de pl ru hi vi id; bbmusic en it fr de pl ru uk hi vi; hyperberry and tengram it fr de es ru uk pl hi id vi ([01-target-and-rules.md](01-target-and-rules.md) section 7).
- Run `lupdate` on the QML as well as the C++: strings in QML use `qsTr`, strings in C++ use `tr()` with the UTF-8 codec from section 5.
- Read the language once at start; a live switch is not built (`ponytail:` comments in `main.cpp`).

## 8. Generated and fetched assets

| Asset | How | Source |
|---|---|---|
| Action and tab icons (`app/assets/images/ic_*.png`) | `python3 scripts/gen-icons.py`: white on transparent, 81 x 81 px (9 du on a Q10), drawn from shape functions `(x, y) -> bool` over the unit square, "with no library but zlib"; run again after changing a shape. bbmusic's also draws the artwork placeholder, the launcher icon and the splash screens (`app/icon.png`, `app/splash-*.png`) and takes `sheet.png` as an argument to write one sheet of all icons | [samples/ui/scripts/gen-icons.py](../samples/ui/scripts/gen-icons.py), bbmusic's [`gen-icons.py`](../samples/ui/scripts/gen-icons-bbmusic.py) (docstrings) |
| Launcher icon and splash screens (phoenix) | `python3 scripts/gen-app-icon.py` from an SVG glyph; needs Pillow and ImageMagick's `magick`; run by hand when the look changes | phoenix's [`gen-app-icon.py`](../samples/ui/scripts/gen-app-icon-phoenix.py) |
| Tengram's icon in every size, launch screens, Hub glyphs | [`scripts/gen-icon.py`](../samples/ui/scripts/gen-app-icon.py) (Pillow) | [samples/ui/scripts/gen-app-icon.py](../samples/ui/scripts/gen-app-icon.py) |
| `cacert.pem` (the engine's TLS roots) | fetched by the build script when missing (`curl -fsSL https://curl.se/ca/cacert.pem`), git-ignored (`app/assets/cacert.pem` in [samples/engine-cxx20/gitignore](../samples/engine-cxx20/gitignore)); lands in `app/native/assets/` | [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh) |
| Emoji font and PNGs | `scripts/fetch-emoji.sh`: Noto Color Emoji pinned to a commit and checked by sha256, a derived font into `app/assets/fonts/`, PNGs into `app/assets/emoji/`. "Nothing here is committed" | [samples/third-party/fetch-emoji.sh](../samples/third-party/fetch-emoji.sh) |
| qrcodegen, OpenMAX IL headers, libwebp, the SQLCipher driver | `scripts/fetch-*.sh`, called at the top of the build script | [samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh), hyperberry's [`build-app.sh`](../samples/ui/scripts/build-app-hyperberry.sh) |
| `libtgs.so` | `scripts/build-tgs.sh` (clang, in the engine image), then linked by the UI | [samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh) |

Pin each download by commit and sha256, and keep fetched files out of git. Icon sizes and splash resolutions in the descriptor: [08-packaging.md](08-packaging.md).

## 9. Build traps

- "Nothing to be done" from `make` after a QML-only change is fine: QML ships as assets ([skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md)).
- After adding to `LIBS`, check qmake regenerated the Makefile: `grep -c lbps build/Makefile` (same skill).
- The packager writes `assets/.assets.index` into the source tree: the build scripts `rm -f` it; git-ignore it.
- Two agents building in parallel use two build folders (`build/ui` and `build/ui-b` in tengram's notes section 4; `build/ui-a`, `build/ui-b` in hyperberry's notes).
- Docker under OrbStack can read a file just edited at its old size (truncated or old content): run again, and after packaging check that the bar holds the change (`unzip -p build/NAME.bar native/assets/X.qml | grep ...`) (hyperberry's notes, 2026-11-01).
- The Mac sleeps during long runs (`caffeinate -i -m -s -t <s>`); OrbStack can hang after sleep (`orbctl stop`, wait 100 s, `orbctl start`) (tengram's notes section 4).
- Docker here is rootless: files written as root in the container belong to the user on the host. Never `chown` inside the container ([skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md)).
- Fixed paths: NDK target `/root/bbndk/target_10_3_1_995/qnx6`, host tools `/root/bbndk/host_10_3_1_12/linux/x86/usr/bin` ([03-toolchains.md](03-toolchains.md)).

## Where sources disagree

- hyperberry's notes say "GCC 4.6.3, so C++98". That holds for phoenix and bbmusic only.
- phoenix's build script pointed `LINT=` at an absolute path inside the hyperberry repo; the sample points it at this repo's `skills/bb10-build-deploy/scripts/cascades-qmllint.sh`.

## Samples

- [samples/ui/app.pro](../samples/ui/app.pro), [samples/ui/app-gcc483.pro](../samples/ui/app-gcc483.pro)
- [samples/ui/scripts/build-app.sh](../samples/ui/scripts/build-app.sh), [samples/ui/scripts/build-ui-checks.sh](../samples/ui/scripts/build-ui-checks.sh)
- [samples/ui/main.cpp](../samples/ui/main.cpp), [samples/ui/main-logging.cpp](../samples/ui/main-logging.cpp), [samples/ui/UiLog.h](../samples/ui/UiLog.h)
- [samples/ui/scripts/gen-icons.py](../samples/ui/scripts/gen-icons.py), [samples/ui/scripts/gen-app-icon.py](../samples/ui/scripts/gen-app-icon.py)
- [samples/descriptor/bar-descriptor-child.xml](../samples/descriptor/bar-descriptor-child.xml), [samples/third-party/fetch-emoji.sh](../samples/third-party/fetch-emoji.sh), [samples/engine-cxx20/gitignore](../samples/engine-cxx20/gitignore)
- This repo: [skills/bb10-build-deploy/SKILL.md](../skills/bb10-build-deploy/SKILL.md), [skills/bb10-build-deploy/scripts/cascades-qmllint.sh](../skills/bb10-build-deploy/scripts/cascades-qmllint.sh), [skills/bb10-cascades/template/hello.pro](../skills/bb10-cascades/template/hello.pro)

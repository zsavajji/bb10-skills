---
name: bb10-build-deploy
description: Build, package, lint, install and debug BlackBerry 10 apps — the 10.3.1 Native SDK in Docker (yamsergey/bb10-ndk:0.6, qcc with GCC 4.6.3 or 4.8.3, qmake, blackberry-nativepackager), the checks that run before every build (lupdate parse, QML handler check, qmllint), bar-descriptor.xml (ids, versions, -buildIdFile, assets, Qnx/Elf, icons, permissions, env, invoke targets, headless entry point and memory class), unsigned .bar installs on rooted phones (installer PPS job over root SSH, Sachesi, DBBT), pathtrust, on-device paths, logs and crash diagnosis, failure signatures. Use when building or packaging a BB10 app, editing a bar descriptor, handing a build to the user for phone testing, or working out why an app quits, keeps its old version, or misbehaves on the phone.
---

# Build, package and install BB10 apps

Scripts: `scripts/` in this skill's folder ([skills/bb10-build-deploy/scripts/](scripts)): `cascades-qmllint.sh`
(with `qmltypes6.py`, `qtquick1.qmltypes`) and `check-qml-handlers.py`. The build pipeline is written up in
`docs/`; this skill keeps the short, correct version and links the doc.
Every phone is rooted, nothing is signed, 10.3.3 is the last OS: [01](../../docs/01-target-and-rules.md).

## Toolchains

| Image | Has | Use for |
|---|---|---|
| `yamsergey/bb10-ndk:0.6` | Native SDK 10.3.1.995: `qcc`, `qmake` (Qt 4.8 + Cascades), `lupdate`, `lrelease`, `blackberry-nativepackager`, `blackberry-deploy`. **linux/386**: on Apple Silicon it runs emulated (Docker warns about the platform) | the Cascades UI, small C shims |
| `bb10-cxx20:19.1.7-5` (pin a release tag, not `latest`; see bb10-cxx20) | clang 19, libc++ 19, armnto binutils, SDK sysroot, `bb10-clang++` | engines, ported libraries (C++20) |

`qcc -V` in the NDK image lists exactly **GCC 4.6.3 and 4.8.3** (no 4.8.4), each with the targets
`gcc_ntoarmv7le`, `_cpp` (Dinkumware C++ library, what Qt and Cascades use), `_cpp-ne` (no exceptions), `_gpp` (GNU libstdc++).

- **Default: GCC 4.6.3** (`qcc -Vgcc_ntoarmv7le`), partial C++11, written as C++98. BBMusic and Phoenix use it. [app, Q10, Classic]
- **GCC 4.8.3 with `-std=gnu++11`**: HyperBerry and Tengram build their UI with it, compiled with the `_cpp` variant and linked
  against 4.6.3's Dinkumware `libcpp`, so one C++ library stays in the process. **Verified on the Q10 and the Classic, 2026-11-20**
  (not "untried with Cascades"). Put these eight lines in the `.pro` and change nothing:

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

  `gnu++11`, not `c++11` (strict ISO hides `open`, `getpid`, `clock_gettime`). C files in the UI: `-std=gnu99`, not `c99`.
  **Trap: clean the build folder once after changing the compiler**; a `.pro` change does not make the objects stale
  (HyperBerry 0.3.1.378 still had 4.6.3's objects). Details: [03](../../docs/03-toolchains.md) section 3.

Docker here is **rootless**: files written as root in the container belong to the user on the host. Never `chown` inside the
container (uid 100999 locks the user out). zsh does not word-split `$vars`; wrap loops in `sh -c`/`bash -c`. The mount covers only
the project: copy files into the project first. A single-quoted `bash -c '...'` block cannot hold an apostrophe.
More Docker traps (sleep, OrbStack, stale file sizes): [03](../../docs/03-toolchains.md) section 4.

## Build a Cascades app

Out of tree, as the apps do ([07](../../docs/07-ui-build.md) section 3):

```sh
docker run --rm -v "$ROOT:/src" -w /src yamsergey/bb10-ndk:0.6 bash -c '
  source /root/bbndk/bbndk-env_10_3_1_995.sh >/dev/null
  set -eu
  mkdir -p build/app-ui
  (cd build/app-ui && qmake -spec blackberry-armv7le-qcc CONFIG+=device ../../app/NAME.pro >/dev/null &&
    make -j"$(nproc)" >/dev/null)
  cd app && blackberry-nativepackager -package ../build/NAME.bar \
    -buildIdFile ../build/app-buildid.txt bar-descriptor.xml -configuration Device-Release
  rm -f assets/.assets.index'
```

- **`-buildIdFile build/app-buildid.txt`** gives every package the next build id: do not bump `<buildId>` by hand. Create the file
  with `1` when it is missing. Compile only (no id spent): the same `qmake` and `make` lines, about 30 s; two builds at once need two build folders.
- The bb10-cascades template has a one-file `build.sh`.
- **`CONFIG += cascades10`** (the Momentics default) builds a release as `o.le-v7/lib<name>.so` for `Qnx/Cascades`; a descriptor that
  expects an executable then fails with `Invalid asset path`. Without it (`TEMPLATE = app` + the libs) the output is a plain
  executable wherever `DESTDIR` says. No Momentics `config.pri`: a self-contained `.pro` builds fine. `main` is `Q_DECL_EXPORT`.
  The `.pro` of a plain-executable app and its optional libs: [07](../../docs/07-ui-build.md) section 1.
  A `.so` started through the launcher (`TEMPLATE = lib`, `CONFIG += plugin`, `Qnx/Cascades`) shows its first page about 3 s
  sooner: [14](../../docs/14-cascades-launcher.md).
- Third-party **C** libraries can be built with the NDK's GCC and linked statically into the UI (`CC=ntoarmv7-gcc AR=ntoarmv7-ar
  RANLIB=ntoarmv7-ranlib ./Configure gcc no-shared no-tests no-asm no-ui-console`). `-Wc,` passes a flag through qcc; `DEFINES`
  reach C++ only, so C defines go through `-Wc,"-D..."` too. Modern **C++** libraries need bb10-cxx20 and a process or C boundary.
- After adding a library to `LIBS`, check qmake regenerated the Makefile (`grep -c lbps build/Makefile`).
- "Nothing to be done" from make after a QML-only change is fine: QML ships as assets.
- The packager writes `assets/.assets.index` into the source tree: `rm -f` it and git-ignore it.
- SDK paths: target `/root/bbndk/target_10_3_1_995/qnx6` (`usr/include`, `armle-v7/usr/lib`, `armle-v7/usr/lib/qt4/imports` with
  the QML `plugins.qmltypes`), host tools `/root/bbndk/host_10_3_1_12/linux/x86/usr/bin`, device profiles
  `/root/bbndk/target_10_3_1_995/blackberry-sdk-descriptor.xml`. `ARG_MAX` on the phone is 61440.
- The Mac editor's red errors for `bb/cascades/...` includes are noise (no SDK on the Mac): the NDK's compile is the check.

## Checks before every build

A QML error makes the app quit at start with nothing on screen, or a page never opens; neither the build nor the phone says why.
Each check came from an incident. Full table, commands and the ready-to-copy block for a build script: [07](../../docs/07-ui-build.md) section 4.

| # | Check | Catches |
|---|---|---|
| 1 | `lupdate app/assets/*.qml -ts /tmp/qml-check.ts 2>&1 \| grep -A2 "error:"` (in the NDK image) | syntax Qt 4.8 cannot parse; the Qt 6 lint lets a `var` inside a binding block through |
| 2 | `grep -n "readonly property" app/assets/*.qml` | `lupdate` accepts it, the phone refuses the page ("Readonly not yet supported") |
| 3 | a function defined twice on a page's root object (the `sed`/`uniq -d` loop in 07 section 4.1) | "Duplicate method name"; `lupdate` does not see it |
| 4 | `python3 scripts/check-qml-handlers.py [PROJECT_ROOT]` | `onX:` for a signal that does not exist ("Cannot assign to non-existent property"); needs the type cache the lint makes |
| 5 | `cascades-qmllint.sh app/assets/*.qml` | misspelt properties, signals, enums; wrong child types; duplicate ids; syntax |
| 6 | flag file `data/check-qml` on the phone | every QML document compiled at start, `check: ok/FAIL X.qml` lines in the UI log (app code in 07 section 4.4) |
| 7 | `node app/tests/*.js` | the app's pure JS |

Chain build steps with `&&`: under `;` an early failure went unnoticed.

### QML lint (check 5)

`scripts/cascades-qmllint.sh` in this skill's folder: Qt 6 qmllint with the NDK's own Cascades type info (converted by `qmltypes6.py`,
cached in `~/.cache/bb10-qmllint`; first run pulls it from the NDK image; on macOS it runs qmllint in a small Debian image).
- **Exit 255** = syntax error or duplicate id: the app would quit at start. Fix before building.
- **Warnings** (exit 0) are real: misspelt properties/signals/enum values ("Did you mean `hintText`?"), wrong child types (an `ActionItem`
  in a `Container`), two children in a `Page`'s single `content`. Read every one.
- `import QtQuick 1.0`: its non-visual `Timer`, `Connections` and `QtObject` are known (`qtquick1.qmltypes`); visual QtQuick items
  (`Rectangle`, `MouseArea`...) are reported as not found, which is right: they don't render in Cascades.
- Known false positives: types registered from C++ (`Timer`), functions declared on a `ListView` and called as `item.ListItem.view.fn()`,
  custom properties read through an id typed as a base type (`page.lid` on a `Page`), `layoutProperties.positionX`, Qt 4 globals
  (`Qt.formatDateTime`, `Qt.openUrlExternally`). Context properties are not checked (`--unqualified disable`).
- It does not check that an API exists in an older import version; 1.4 is the newest.

### Handler check (check 4)

`scripts/check-qml-handlers.py [PROJECT_ROOT]` (default: the current directory; reads `PROJECT_ROOT/app/assets/*.qml`). Every `onSomething:`
must be a signal of the element it sits in; it reads QML by layout (one element or property per line) and fails when it cannot place a
handler. `--self-test` proves it still finds three kinds of bad handler. Types an app registers from C++ go in the `EXTRA` table at the
top, or copy the script into the app's own `scripts/` and edit it there. It needs the cache `cascades-qmllint.sh` makes; run the lint once.

## bar-descriptor.xml

```xml
<id>io.github.user.app</id>            <!-- new id = new app beside the old; uninstall deletes its data -->
<versionNumber>1.2.0</versionNumber>   <!-- up to 3 parts, each 0-999 -->
<buildId>1</buildId>                   <!-- 0-65535; -buildIdFile overwrites it at packaging; Package-Version = versionNumber.buildId -->
<initialWindow><autoOrients>false</autoOrients><aspectRatio>portrait</aspectRatio>
  <systemChrome>none</systemChrome></initialWindow>
<configuration name="Device-Release">
  <platformArchitecture>armle-v7</platformArchitecture>
  <asset entryPointId="App" path="../build/app-ui/app/app" entry="true" type="Qnx/Elf">app</asset>
  <asset path="../build/lib/libx.so" type="Qnx/Elf">lib/libx.so</asset>   <!-- every ELF: Qnx/Elf -->
</configuration>
<entryPoint id="App"><name>App</name></entryPoint>
<asset path="assets">assets</asset>    <!-- path is relative to app/ (the packager runs there); lands in app/native/assets = asset:/// -->
<icon><image>icon.png</image></icon>   <!-- 90 for a Q10; HyperBerry and Tengram ship 86, 90, 96, 110, 144 -->
<permission system="true">run_native</permission>
<env var="LD_LIBRARY_PATH" value="app/native/lib:/usr/lib/qt4/lib"/>
<env var="CASCADES_THEME" value="dark?primaryColor=0xRRGGBB&amp;primaryBase=0xRRGGBB"/>
<invoke-target id="io.github.user.app">  <!-- other apps can open http(s) links here -->
  <invoke-target-type>application</invoke-target-type>
  <filter><action>bb.action.OPEN</action><action>bb.action.VIEW</action>
    <mime-type>*</mime-type><property var="uris" value="http://,https://"/></filter>
</invoke-target>
```

- Assets map `path` (in the source tree) to a path under `app/native/`. Ship shared libraries under their SONAME (`libc++_shared.so.1`):
  symlinks are not carried. Only `Qnx/Elf` assets get the execute bit, so **every ELF (executables, `.so`, helpers) needs `type="Qnx/Elf"`**.
- **Headless engine** (second entry point; the exact XML is in [08](../../docs/08-packaging.md) section 2): an asset with its own
  `entryPointId` and no `entry="true"`; an `<entryPoint>` with `<entryPointType>Qnx/Elf</entryPointType>`; system permissions
  `_sys_run_headless`, `_sys_headless_nostop` and a memory class; an `application.headless` invoke target with a custom
  `<id>.START` action and `<property var="uris" value="data://local"/>`. **Without a memory class the heap cap is 3 MiB**; classes:
  `_sys_headless_medium` 6, `_sys_headless_large` 10, `_sys_headless_xlarge` 30 MiB (128 MiB once the `rlimit-patch` addon runs).
  Start-at-boot, the 6 s navigator rule, the cap and the patch: [09](../../docs/09-headless-service.md).
- **Package id and version**: unsigned packages get an id of the form `<id>.testRel_<name><hash>`; check `Package-Version` in the
  manifest (below). Packaging: [08](../../docs/08-packaging.md) sections 3 and 6.
- A permission left out of the descriptor makes its API quietly do nothing: when code that looks right does nothing, check the
  permissions first. Seen in use: `access_internet`, `access_shared` (the user can refuse: fall back to `data/`),
  `run_when_backgrounded`, `post_notification`, `use_camera`, `record_audio`, `play_audio`, `access_pimdomain_contacts`,
  `_sys_access_pim_unified` (Hub account), plus `read_geolocation`, `access_location_services`, `read_device_identifying_information`,
  `access_pimdomain_calendars|messages|notebooks`, `access_led_control`, `access_phone`, `access_sms_mms` in third-party samples.
  List with sources: [08](../../docs/08-packaging.md) section 5.
- No API makes an app the default browser/handler; the user picks "Set as default" in the Open in... chooser (untested).

## Versions: every build gets a new build id

The installer keeps the installed app when Package-Version did not change. Three rounds of fixes were once "tested" on a stale build.
Before handing over:

```sh
unzip -p build/app.bar META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version)'
unzip -p build/app.bar native/assets/main.qml | grep -c 'something you just changed'
```

When a phone log disagrees with the code, first compare the build/arguments the log reports with what was built.

## Install (no signing)

Every phone is rooted and runs unsigned packages (signing servers died in January 2022): `blackberry-nativepackager -package` without
`-sign`, no `-devMode`. Never spend time on keystores, author certificates or debug tokens. Routes, in order of use:

1. **The installer's PPS job object over root SSH** ([10](../../docs/10-phone-access-and-install.md) section 4). **An agent uses it only
   when the user says so, each time**; approval does not carry over. Recipe, from [samples/phone/install-phone.sh](../../samples/phone/install-phone.sh):
   ```sh
   scp -q bar.bar bb10rootx:/var/rootdata/N.bar
   ssh bb10rootx 'chmod 644 /var/rootdata/N.bar
     J=/pps/system/installer/upd/current/job.N.bar
     umask 000; touch $J; chmod 666 $J
     echo "action::install" >> $J; echo "package_location::/var/rootdata/N.bar" >> $J; echo "extras::" >> $J
     i=0; while [ $i -lt 90 ]; do grep "^result::" $J >/dev/null 2>&1 && break; sleep 1; i=$((i+1)); done
     grep -E "^(result|errorstr|error|message|actual_version)" $J; rm -f /var/rootdata/N.bar $J'
   ```
   `N` is a unique tag. A good install prints `result::success` and `actual_version::<versionNumber>.<buildId>`. The phone has no `printf`: use `echo >>`.
2. **The user's own route**: Sachesi or DBBT with Development Mode on (Settings, Security and Privacy). Hand over the `.bar`.
3. **`blackberry-deploy` / BarDeploy from the Mac**: exists in the SDK, tried 2026-10-31 and **never completed end to end**
   ([10](../../docs/10-phone-access-and-install.md) section 6).

- **An install closes the app and its engine**; the user reopens the app. Install only when it will not interrupt the user.
- **The version must go up**, or the installer keeps the old app. Data survives an upgrade; only an uninstall deletes `data/`.
- **Addons** (root installer zips such as `headless-mem.zip`) install with `/var/rootdata/installer <zip>`; log `/var/log/installer.log`. Ask first.
- **`ssh bb10rootx` is the cable, not a phone**: it reaches the Q10 or the Classic. Check
  `ssh bb10rootx 'grep modelname /pps/services/deviceproperties'` before naming the phone.
- **Running a test binary by hand**: a new file runs only after `/proc/boot/pathtrust !<file>` (always the `!`; a refused file stays
  refused, even on a reused inode); [samples/phone/push.sh](../../samples/phone/push.sh) shows the pattern ([10](../../docs/10-phone-access-and-install.md) section 3).
  Root cannot execute an installed app's binaries: test an installed build through the navigator.

## On the phone

| Path | What |
|---|---|
| `/accounts/1000/appdata/<id>.testRel_<name><hash>/` | the app's sandbox (also under `/apps/...`) |
| `.../app/native/` | package contents, read-only; `QCoreApplication::applicationDirPath()` |
| `.../data/` | writable; `QDir::homePath()` (settings, databases, caches, logs) |
| `/accounts/1000/shared/misc` | "misc" in File Manager: logs and flag files the user can reach (`access_shared`) |
| `/accounts/1000/shared/downloads`, `documents`, `photos`, `camera`... | shared folders (`access_shared`) |
| `/apps/<pkg>/META-INF/MANIFEST.MF` | the installed version |
| `/apps/<pkg>/public/` | `public="true"` assets, for processes outside the sandbox (the Hub) |
| `/var/addons/<name>` | root-installer addons |
| `/base/usr/lib/graphics/msm8960/` | Adreno drivers (Q10) |

The phone's shell lacks `head`, `tr`, `uniq`, `strings`, `printf`, `diff`, `sqlite3`, `grep -A/-B`: filter on the Mac (`ssh bb10rootx 'cat f' > local`).

## Logs and crashes

- **With root SSH** (the usual case now): the apps log in the sandbox, `data/logs/<app>d.log` and `data/logs/ui.log` (HyperBerry,
  Tengram), `ui.log` (Phoenix), `bbmusic-ui.log` and `bbmusicd.log` (BBMusic); agents read them over root SSH. `slog2info -b <app>`
  has the system log. Per-app table: [11](../../docs/11-testing-and-debugging.md) section 3.
- **Without SSH**: write a timestamped log to `/accounts/1000/shared/misc/<app>.log` (fallback `data/`), capped (past 2 MB keep the
  newest 1 MB), with one line per start giving the version/build, size and arguments. The user copies it to the PC (into the project's
  `debug/`; once it landed in `build/`: look in both). Needs `access_shared`.
- Check a log's mtime and that it covers the build in question before concluding anything.
- `qDebug`/`qWarning` go nowhere on the phone unless the app routes them (`qInstallMsgHandler` to a file and the system log); QML
  errors included. Flush every line: a freeze or kill keeps what was written.
- A headless engine's stderr goes nowhere: open its log file first thing. A child process's stdout/stderr can go to the same file.
- BB10 can kill an app without a clean exit: flush files and settings as you go.
- Without a cable: a small HTTP server on the PC on the same Wi-Fi that the phone posts lines to.

### Failure signatures

| What you see | Likely cause |
|---|---|
| App quits at start, nothing on screen | a QML error in the root document: `QML:` lines in the UI log, lint, `lupdate` ([07](../../docs/07-ui-build.md)) |
| A page never appears or opens black | that page's document failed (unknown signal, duplicate function, `readonly property`, `Component.onCompleted`, `bb.system` type with `onCreationCompleted`); the `data/check-qml` run names it |
| `Invalid asset path` | a `cascades10` build (`.so`) with an executable descriptor |
| `Process N (name) terminated SIGSEGV code=1 fltno=11 ip=...` in a shell; "Process crashed" from QProcess | a native crash; `FATAL:... Check failed` + SIGTRAP is Chromium code |
| First `throw` crashes in `libz.so.2@_Unwind_GetLanguageSpecificData` | a bb10-cxx20 image older than v19.1.7-4 bound the unwinder to `libz`: pin v19.1.7-4 or later ([04](../../docs/04-engine-cxx20.md) section 3; see bb10-cxx20) |
| Builds, then "symbol not found" on the phone only | an import missing from `libc.so.3`/`libm.so.2` |
| **Headless engine exits before `main` (about 47 ms), the UI's invoke gets error 6 `Target`, or `std::bad_alloc`** | the headless memory cap: no memory class (3 MiB) or the 30 MiB class ([09](../../docs/09-headless-service.md)) |
| **Engine slain about 6 s after launch** | it did not reach the navigator in time; shows only in `slog2info \| grep navigator` |
| **Engine exits 60-80 ms after launch** | it took the navigator's arguments (package name, `invoke://localhost`) for an error: log and ignore them |
| Engine never starts after START is refused, or CPU busy for hours | a background app cannot invoke ("not allowed to make invoke requests", reply `InvokeReplyError` 2 `BadRequest`); back off instead of retrying |
| **"Operation not permitted" / exit 126 running a binary** | not trusted: `pathtrust` ([10](../../docs/10-phone-access-and-install.md) section 3) |
| Old behaviour after an install | Package-Version unchanged; check `actual_version::` / `MANIFEST.MF` |
| An addon install "does nothing" | the app kept its first-launch limit: close every process of the app once |
| Nothing happens though the code looks right | a permission missing from the descriptor |

More signatures (SIGFPE after OpenSSL init, and so on): [11](../../docs/11-testing-and-debugging.md) section 4.

## Handing a build to the user

1. Checks pass (exit 0, warnings read). 2. Build and package (`-buildIdFile` bumps the id). 3. Check `Package-Version` in the manifest and
grep one changed QML line inside the bar. 4. Give the path of the `.bar` and a short numbered list of what to try on the phone, naming which
build each item needs. Say "builds", not "works", until the phone confirms it. Details: [11](../../docs/11-testing-and-debugging.md) section 6.

# Packaging: bar-descriptor.xml and the .bar

Read this when: you write or change a `bar-descriptor.xml`, add a permission or an invoke target, package a build, or need to know where an installed app's files are on the phone.

Paths in parentheses are relative to the `bb10/` folder. Build steps before packaging: [07-ui-build.md](07-ui-build.md). Installing the result: [10-phone-access-and-install.md](10-phone-access-and-install.md). The headless service the second process needs: [09-headless-service.md](09-headless-service.md). Signing is dead; packages are unsigned ([01-target-and-rules.md](01-target-and-rules.md) section 3).

## 1. A complete descriptor for a new two-process app

Start from `phoenix/app/bar-descriptor.xml`: the engine is a plain child process of the UI (a `Qnx/Elf` asset, not an entry point). Copied exactly:

```xml
<?xml version="1.0" encoding="utf-8" standalone="no"?>
<qnx xmlns="http://www.qnx.com/schemas/application/1.0">
    <id>com.zsavajji.phoenix</id>
    <name>Phoenix for Reddit</name>
    <versionNumber>0.1.0</versionNumber>
    <!-- Overwritten at packaging by -buildIdFile (scripts/build-app.sh). -->
    <buildId>1</buildId>
    <description>An unofficial Reddit client.</description>
    <author>zsavajji</author>
    <!-- 90 x 90, the Q10's size; drawn by scripts/gen-icons.py. -->
    <icon>
        <image>icon.png</image>
    </icon>
    <asset path="icon.png">icon.png</asset>
    <!-- Shown from the tap to the app's first frame; one per screen the app meets in portrait
         (Q10/Q5/Q20, Z30, Z10, Passport); drawn by scripts/gen-icons.py. -->
    <asset path="splash-720x720.png">splash-720x720.png</asset>
    <asset path="splash-720x1280.png">splash-720x1280.png</asset>
    <asset path="splash-768x1280.png">splash-768x1280.png</asset>
    <asset path="splash-1440x1440.png">splash-1440x1440.png</asset>
    <splashScreens>
        <image>splash-720x720.png</image>
        <image>splash-720x1280.png</image>
        <image>splash-768x1280.png</image>
        <image>splash-1440x1440.png</image>
    </splashScreens>

    <initialWindow>
        <autoOrients>false</autoOrients>
        <aspectRatio>portrait</aspectRatio>
        <systemChrome>none</systemChrome>
    </initialWindow>

    <configuration name="Device-Release">
        <platformArchitecture>armle-v7</platformArchitecture>
        <asset entryPointId="Phoenix" path="../build/app-ui/app/phoenix" entry="true" type="Qnx/Elf">phoenix</asset>
        <!-- The engine: not an entry point, a plain child of the UI (app/native/phoenixd). -->
        <asset path="../build/engine-bb10/phoenixd" type="Qnx/Elf">phoenixd</asset>
    </configuration>

    <entryPoint id="Phoenix">
        <name>Phoenix for Reddit</name>
    </entryPoint>

    <!-- The QML and cacert.pem: app/native/assets. -->
    <asset path="assets">assets</asset>
    <!-- The translations, compiled by scripts/build-app.sh: app/native/qm. -->
    <asset path="../build/qm">qm</asset>

    <permission system="true">run_native</permission>
    <permission>access_internet</permission>
    <permission>access_shared</permission>
    <env var="LD_LIBRARY_PATH" value="app/native/lib:/usr/lib/qt4/lib"/>
    <!-- Dark for the Q10's OLED; orange as the primary colour. -->
    <env var="CASCADES_THEME" value="dark?primaryColor=0xFF6A1A&amp;primaryBase=0xD9480F"/>
</qnx>
```

Reading it:

| Element | Meaning |
|---|---|
| `<id>`, `<name>`, `<description>`, `<author>` | identity; `id` is reverse-DNS and fixed for good (section 3) |
| `<asset path="X">Y</asset>` | `path` is a file or folder in the source tree, relative to `app/` (the packager runs there); the text is where it lands under `app/native/` on the phone. `path="assets">assets` ships the whole folder, read as `asset:///` |
| `entry="true"` plus `entryPointId` | the UI executable the launcher starts; its id matches `<entryPoint id>` |
| `type="Qnx/Elf"` | every ELF file needs it (section 3) |
| `<configuration name="Device-Release">` | binaries are listed inside it; the packaging command ends `-configuration Device-Release` |
| `<initialWindow>` | portrait only, no system chrome: keyboard phones are portrait only ([01-target-and-rules.md](01-target-and-rules.md) section 6) |
| `<env LD_LIBRARY_PATH>` | `app/native/lib` (your shipped libs) then `/usr/lib/qt4/lib`; the Hub library, loaded by `dlopen`, pulls in QtCore and QtGui and needs the second entry (`tengram/docs/ui-fork.md` section 5) |
| `<env CASCADES_THEME>` | `dark?primaryColor=0xRRGGBB&amp;primaryBase=0xRRGGBB`: dark for the OLED phones (Q10, Z30, P'9983), an accent colour per app. It "applies before any code runs" (`hyperberry/docs/plan/pitfalls.md`). `&` is written `&amp;` in XML |

Accents in use: phoenix `0xFF6A1A`/`0xD9480F`; bbmusic `0xFF0000`/`0xCC0000`; hyperberry `0x12543A`/`0x1BD862`; tengram `0x24A1DE`/`0x1B79A7` (the four descriptors).

## 2. The headless additions (diff from `bbmusic/app/bar-descriptor.xml`)

`bbmusic` is phoenix's descriptor with the engine made a second entry point. Only the differences, copied from `diff phoenix/app/bar-descriptor.xml bbmusic/app/bar-descriptor.xml` (identity, texts and theme colour left out):

```xml
 <configuration name="Device-Release">
     <platformArchitecture>armle-v7</platformArchitecture>
+    <asset entryPointId="BBMusic" path="../build/app-ui/app/bbmusic" entry="true" type="Qnx/Elf">bbmusic</asset>
+    <asset entryPointId="BBMusicService" path="../build/engine-bb10/bbmusicd" type="Qnx/Elf">bbmusicd</asset>
 </configuration>

 <entryPoint id="BBMusic">
     <name>BBMusic</name>
 </entryPoint>
+<!-- The engine, headless, as the gate app's: a memory class (else the heap is capped at 3 MB)
+     and no stop when the UI is swiped away. Not started at boot. -->
+<entryPoint id="BBMusicService">
+    <name>BBMusic Engine</name>
+    <entryPointType>Qnx/Elf</entryPointType>
+    <permission system="true">_sys_run_headless</permission>
+    <permission system="true">_sys_headless_nostop</permission>
+    <permission system="true">_sys_headless_xlarge</permission>
+</entryPoint>

+<invoke-target id="com.zsavajji.bbmusic.service">
+    <invoke-target-type>application.headless</invoke-target-type>
+    <entry-point-id>BBMusicService</entry-point-id>
+    <filter>
+        <action>com.zsavajji.bbmusic.START</action>
+        <mime-type>*</mime-type>
+        <property var="uris" value="data://local"/>
+    </filter>
+</invoke-target>

 <permission>access_internet</permission>
+<permission>run_when_backgrounded</permission>
+<permission>play_audio</permission>
```

How these parts relate: the engine asset carries `entryPointId="BBMusicService"` and no `entry="true"`; `<entryPoint id="BBMusicService">` gives it `<entryPointType>Qnx/Elf</entryPointType>` and its own system permissions; the `application.headless` invoke target names that entry point and a custom action (`...START`) with `uris` = `data://local`, which is what the UI's `InvokeRequest` matches. The invoke code, memory classes, start at boot and the cap itself are in [09-headless-service.md](09-headless-service.md). bbmusic keeps its engine out of boot start on purpose ("Not started at boot").

## 3. Rules, each with its source

| Rule | Source and reason |
|---|---|
| **The `<id>` is fixed for good.** A new id is a different app beside the old; uninstalling deletes `data/` | "Fixed for good: a new id is a different app beside this one, and uninstalling deletes data/ (session.db holds the link, chats.db the only copy of the history)" (`hyperberry/app/bar-descriptor.xml`); tengram's says `db/` holds the Telegram session |
| `versionNumber`: up to 3 parts, each 0-999 | `hyperberry/.claude/skills/bb10-build-deploy/SKILL.md` (only source) |
| `buildId`: 0-65535; `Package-Version` = `versionNumber.buildId` | same skill. Phoenix/bbmusic keep `<buildId>1</buildId>` as a placeholder |
| **`-buildIdFile` overwrites `buildId` at packaging, and every package must get a new one** | "the installer keeps the old app when Package-Version (versionNumber.buildId) didn't change" (`hyperberry/app/bar-descriptor.xml`). "Three rounds of fixes were once 'tested' on a stale build" (skill). Scripts keep `build/app-buildid.txt` (phoenix, bbmusic) or `build/buildid.txt` (hyperberry, tengram), create it with `1` when missing, and print `Package-Version` |
| **Every ELF asset gets `type="Qnx/Elf"`** | only `Qnx/Elf` assets get the execute bit; a binary copied to shared storage loses it (skill). That includes `.so` files and helper executables |
| **Ship shared libraries under their SONAME** | symlinks are not carried (skill, example `libc++_shared.so.1`) |
| **Assets land under `app/native/`** | `path` in source, text = destination (`phoenix/app/bar-descriptor.xml` comments) |
| **A permission left out fails silently** | the API quietly does nothing: when code that looks right does nothing on the phone, check permissions first (skill; `hyperberry/docs/plan/pitfalls.md`; `tengram/docs/ui-fork.md` section 5) |
| Not signed, no `-devMode` | section 5 |

## 4. Extras of hyperberry and tengram

Both are one descriptor shape (`hyperberry/app/bar-descriptor.xml`, `tengram/app/bar-descriptor.xml`); tengram adds the multi-file share, `libtgs.so` and a different Hub glyph set.

**Service filters** (on `application.headless` target `<id>.service`), beside `...START`:

| Filter | What it is |
|---|---|
| `bb.action.system.TIMER_FIRED`, mime `*`, `uris` `data://local` | start at boot through a recurring timer ([09-headless-service.md](09-headless-service.md)) |
| `<id>.MARKREAD`, `.MARKUNREAD`, `.DELETE`, mime `application/vnd.zsavajji.<app>.chat`, `uris` `data://,pim:` | the Hub items' Mark as read, Mark as unread and Delete |

**UI invoke target** (`application`, entry point = the UI), filters:

| Action(s) | mime | `uris` | Purpose and trap |
|---|---|---|---|
| `bb.action.OPEN`, `bb.action.VIEW` | `application/vnd.zsavajji.<app>.chat` | `pim:` | a tap on a chat's Hub item. "Without the pim: uri a tap matches nothing" (comment in both) |
| `bb.action.SHARE` | `*` | `file://` | the phone's Share menu: a file of any type. Taken by `src/OpenRequests.cpp`, a page picks the chat |
| `bb.action.SHARE` | `filelist/*` | `list://` | the system's multi-file share: the data is a list of file uris (tengram only; `src/OpenRequests.cpp`) |
| `bb.action.SHARE` | `text/plain` | `data://local` | shared text |
| `bb.action.OPEN`, `bb.action.VIEW` | `*` | `http://wa.me/,https://wa.me/,...,whatsapp://` (hyperberry only; a long comma-separated list in the file) | links tapped anywhere. "A more specific uri wins over the browser's http://" |

The skill's generic example registers `http://,https://` to receive web links from other apps (skill); hyperberry registers specific prefixes instead (`hyperberry/app/bar-descriptor.xml`). "No API makes an app the default browser/handler; the user picks 'Set as default' in the Open in... chooser (untested)" (skill).

**Card target** (bbmusic, `bbmusic/app/bar-descriptor.xml`, `bbmusic/docs/signin-card.md`):

```xml
<invoke-target id="com.zsavajji.bbmusic.card.signin">
    <invoke-target-type>card.picker</invoke-target-type>
    <entry-point-id>BBMusic</entry-point-id>
    <filter>
        <action>com.zsavajji.bbmusic.SHARE_SIGN_IN</action>
        <mime-type>application/json</mime-type>
        <property var="uris" value="data://local"/>
    </filter>
</invoke-target>
```

The card runs the UI's own entry point; `bbmusic/app/src/main.cpp` detects it with `InvokeManager::startupMode() == ApplicationStartupMode::InvokeCard` and loads `SignInCard.qml` instead of `main.qml`. The answer goes back with `childCardDone`; calling a card must be done from C++ because "QML's `Invocation` cannot read a card's answer" (`bbmusic/docs/signin-card.md`).

**Public assets for the Hub** (hyperberry, tengram):

```xml
<!-- The Hub is another process: it reads these from /apps/<pkg>/public/ (engine/hub_qnx.go
     hands it that folder). -->
<asset path="assets/images/hub.png" public="true">hub.png</asset>
```

A glyph that is white vanished on the light Hub; icons must show on it (`hyperberry/docs/plan/pitfalls.md`, "A UDS Hub account").

**A shared library** (tengram):

```xml
<asset path="../build/prefix/lib/libtgs.so" type="Qnx/Elf">lib/libtgs.so</asset>
```

It lands in `native/lib`, which is on `LD_LIBRARY_PATH` (section 1).

**Helper executables and bundled files** (hyperberry, tengram):

```xml
<asset path="../build/ui/tg-install-patcher" type="Qnx/Elf">tg-install-patcher</asset>
<asset path="../build/headless-mem.zip">headless-mem.zip</asset>
```

The helper is compiled with `qcc -Vgcc_ntoarmv7le -O2 -o build/ui/tg-install-patcher app/helper/tg-install-patcher.c` inside the NDK image (`tengram/scripts/build-app.sh`). What they do: [09-headless-service.md](09-headless-service.md).

**Icons: five sizes and splash screens.** hyperberry and tengram list `icon-86.png`, `icon-90.png`, `icon-96.png`, `icon-110.png`, `icon-144.png` as five `<asset>`s and five `<icon><image>` entries ("every size the home screens pick from. The Q10 takes 90"). phoenix and bbmusic ship one 90 x 90 `icon.png`. Splash screens are one per resolution the app can meet in portrait: 720x720 (Q10, Q5, Classic), 720x1280 (Z30), 768x1280 (Z10), 1440x1440 (Passport). The launch screen is shown "from the tap until the app's first frame (the user's ask, 2026-11-06: it stayed black)". Drawn by the `gen-*.py` scripts ([07-ui-build.md](07-ui-build.md) section 8).

## 5. Permissions

A missing permission makes the API silently do nothing. The user grants the non-system ones on first start and can change them later in Settings, Security and Privacy, Application Permissions: code must work, or say why it cannot, when one is refused (skill).

| Permission | Used by | What for (source comment, or "no comment") |
|---|---|---|
| `run_native` (`system="true"`) | all four | native executable; no comment |
| `access_internet` | all four | network. "Declared here only, like access_internet, which the engine uses too" (`bbmusic/app/bar-descriptor.xml`): the engine's use is covered by the app's declaration |
| `access_shared` | all four | shared folders. hyperberry/tengram: "Downloads, for the chat export (step 5.8b) and Phase 6's files". bbmusic: the engine copies the cover for the volume overlay into shared storage because the overlay (`/usr/sbin/nowplaying`, group `1000_shared`) cannot read the app's own folder. phoenix: no comment. The memory-patch UI writes its result file to `/accounts/1000/shared/documents/<app>-memory-patch.txt` (`hyperberry/app/src/MemoryPatch.cpp`). The user can refuse it: fall back to `data/` (skill) |
| `run_when_backgrounded` | bbmusic, hyperberry, tengram | keep running when minimised; no comment in the descriptors. [09-headless-service.md](09-headless-service.md) says what else keeps an engine alive |
| `play_audio` | bbmusic, hyperberry, tengram | sound output; no comment |
| `post_notification` | hyperberry, tengram | notifications |
| `record_audio` | hyperberry, tengram | microphone (voice notes, calls) |
| `use_camera` | hyperberry, tengram | camera (calls, video) |
| `access_pimdomain_contacts` | hyperberry, tengram | "The phone's address book, read by the UI (src/ContactBook.cpp) and handed to the engine, so a person saved on the Q10 shows under that name" |
| `_sys_access_pim_unified` (`system="true"`) | hyperberry, tengram | the Hub account. "The name is from BBTelega's bar, an unsigned app with a working Hub account on the Q10." Never prompted (`hyperberry/docs/plan/pitfalls.md`) |
| `_sys_run_headless`, `_sys_headless_nostop`, `_sys_headless_xlarge` (`system="true"`, on the service `<entryPoint>`) | bbmusic, hyperberry, tengram | headless entry point, no stop on swipe-away, the largest memory class. See [09-headless-service.md](09-headless-service.md) |

Others the skill lists as seen in third-party samples, not used by these four: `read_geolocation`, `access_location_services`, `read_device_identifying_information`, `access_pimdomain_calendars|messages|notebooks`, `access_led_control`, `access_phone`, `access_sms_mms`. The phone grants `_sys_headless_xlarge` to an unsigned app, "as it does `_sys_headless_nostop`" (`hyperberry/docs/plan/phase0.md`, 0.3).

## 6. The packaging command

Run from `app/`, inside the NDK image after `source /root/bbndk/bbndk-env_10_3_1_995.sh`, exactly as `phoenix/scripts/build-app.sh` does:

```sh
cd app && blackberry-nativepackager -package ../build/phoenix.bar \
	-buildIdFile ../build/app-buildid.txt bar-descriptor.xml -configuration Device-Release
rm -f assets/.assets.index
```

- No `-sign`. The package is unsigned; the package id looks like `com.x.app.testRel_...` (skill: "`Package-Id: testRel_...`"; on the phone the folder is `com.zsavajji.tengram.testRel_jji_tengram30796029`, `tengram/docs/HANDOFF.md` section 3).
- No `-devMode` in any of the four build scripts. The older skill notes: "An earlier app packaged without `-devMode` and installed fine; some third-party apps use `-devMode` (needed to reach app data remotely)". Do not add it.
- `-buildIdFile` reads the next build id from the file ([section 3](#3-rules-each-with-its-source)).

Verify before handing over (the build scripts print the first two):

```sh
unzip -p build/app.bar META-INF/MANIFEST.MF | grep -E '^Package-(Id|Version):'
unzip -l build/app.bar
unzip -p build/hyperberry.bar native/assets/X.qml | grep ...
```

`Package-Version` must be higher than what is installed. The third line proves a changed QML line is in the bar (Docker under OrbStack can package a file at its old size). Checklist for handing over: [11-testing-and-debugging.md](11-testing-and-debugging.md).

Size: the engine is inside the bar; tengram's static `tengramd` is 37 MB (`tengram/docs/HANDOFF.md` section 4).

## 7. Paths of an installed app on the phone

| Path | What |
|---|---|
| `/accounts/1000/appdata/<id>.testRel_<name><hash>/` | the app's sandbox. Seen: `com.zsavajji.tengram.testRel_jji_tengram30796029`, `com.zsavajji.bbmusic.testRel_jji_bbmusic7322f61a` (the skill writes the shorter pattern `<id>.testRel_<name><hash>`); also reachable under `/apps/...` |
| `.../app/native/` | package contents, read-only. `QCoreApplication::applicationDirPath()` in the UI. phoenix also loads translations by the relative path `app/native/qm`, and `MemoryPatch.cpp` builds `QDir::currentPath() + "/app/native/..."`: the app runs from the sandbox root |
| `.../data/` | writable. `QDir::homePath()` in the UI; `HOME` in the engine. Settings, databases, `engine.json`, logs. Survives an upgrade, deleted by an uninstall |
| `/accounts/1000/shared/{misc,downloads,documents,photos,camera,...}` | shared folders (`access_shared`); `misc` is "File Manager's misc: logs and flag files the user can reach" |
| `/apps/<pkg>/public/` | the `public="true"` assets, for processes outside the sandbox (the Hub) |
| `/apps/<pkg>/META-INF/MANIFEST.MF` | the installed version: `grep Package-Version /apps/com.zsavajji.hyperberry*/META-INF/MANIFEST.MF` (`hyperberry/docs/memory/q10-root-ssh.md`) |
| `/var/addons/<name>` | root-installer addons ([09-headless-service.md](09-headless-service.md)) |

Sources: `hyperberry/.claude/skills/bb10-build-deploy/SKILL.md` ("On the phone"), `tengram/docs/HANDOFF.md` section 3.

## 8. Secrets at build time

The mechanism (nothing here opens a key file): an API key and hash (Telegram's) come from the environment or from a git-ignored key file at the repo root, are read by the build script, and are compiled into the engine with `-D`. They are never in a tracked file, notes or memory (`tengram/docs/HANDOFF.md` section 3).

- `tengram/scripts/build-app.sh` exports `TG_API_ID` and `TG_API_HASH` from the environment, or reads `TG_API_ID=` and `TG_API_HASH=` lines of `.tg-keys` with `sed -n 's/^TG_API_ID=//p'`; it stops with exit 2 and a message when either is missing.
- `tengram/engine/build.sh` passes them into the Docker build as `-e TG_API_ID=... -e TG_API_HASH=...` and compiles `-DTENGRAM_API_ID=$TG_API_ID -DTENGRAM_API_HASH="$TG_API_HASH"`. The packaged service gets no environment of ours, so they must be compiled in. The engine lets `TG_API_ID`/`TG_API_HASH` in the environment override them for test runs (`tengram/engine/main.cpp`).
- The key file is in `.gitignore` (`tengram/.gitignore`); phoenix's is `.reddit-keys` (`phoenix/.gitignore`).
- Ask the user for them; pass on the command line only (`TG_API_ID=... TG_API_HASH=... scripts/build-app.sh`).

## 9. Bundling a sibling repo's artifact

hyperberry and tengram bundle `../rlimit-patch`'s `headless-mem.zip`. The Docker mount covers only the project (`-v "$ROOT:/src"`), so the zip must be a real copy inside the project's `build/` before the packager runs. From `tengram/scripts/build-app.sh`:

```sh
if [ ! -x "$ROOT/../rlimit-patch/build.sh" ]; then
	echo "build-app.sh: ../rlimit-patch is missing: the package bundles its headless-mem.zip" >&2
	exit 2
fi
"$ROOT/../rlimit-patch/build.sh" >/dev/null
cp "$ROOT/../rlimit-patch/build/headless-mem.zip" "$ROOT/build/headless-mem.zip"
```

The descriptor then says `<asset path="../build/headless-mem.zip">headless-mem.zip</asset>` (path relative to `app/`). The same applies to any file outside the project root.

## Where sources disagree

- `tengram/app/bar-descriptor.xml` says `-buildIdFile` is used by `scripts/build-ui.sh`; the packaging is in `tengram/scripts/build-app.sh`. The script wins.
- hyperberry's phase-0 notes say the boot filter was `bb.action.system.STARTED`; the descriptors use `bb.action.system.TIMER_FIRED`. See [09-headless-service.md](09-headless-service.md).
- The skill's `<permission>` listing and `-devMode` note are the older text; the four descriptors and build scripts are newer and used here.

## Sources

- `phoenix/app/bar-descriptor.xml`, `bbmusic/app/bar-descriptor.xml`, `hyperberry/app/bar-descriptor.xml`, `tengram/app/bar-descriptor.xml`
- `phoenix/scripts/build-app.sh`, `bbmusic/scripts/build-app.sh`, `hyperberry/scripts/build-app.sh`, `tengram/scripts/build-app.sh`, `tengram/engine/build.sh`
- `hyperberry/.claude/skills/bb10-build-deploy/SKILL.md` (older), `hyperberry/docs/plan/pitfalls.md`, `hyperberry/docs/plan/phase0.md`
- `tengram/docs/HANDOFF.md`, `tengram/docs/ui-fork.md`, `bbmusic/docs/signin-card.md`, `bbmusic/app/src/main.cpp`
- `hyperberry/app/src/MemoryPatch.cpp`, `hyperberry/docs/memory/q10-root-ssh.md`, `phoenix/.gitignore`, `tengram/.gitignore`

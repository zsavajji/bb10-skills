# Official Cascades samples, by topic

BlackBerry's Cascades samples, https://github.com/blackberry/Cascades-Samples (Apache 2.0); paths below are repo-relative.
Each folder is a complete project (`.pro`, `bar-descriptor.xml`, `src/`, `assets/`);
`docs/<name>.qdoc` explains it. Most use `bb.cascades 1.0`–`1.3`; `belt` and `highlighter`
use `1.4`. Read the sample before inventing a pattern: these are the platform's idioms.

| Topic | Samples |
|---|---|
| Every control, look and feel | `cascadescookbookqml` (QML), `cascadescookbookcpp` (same in C++) |
| Starter apps | `hellocascades`, `BfB-Boilerplate-10.1`, `BfB-Boilerplate-10.2`, `BfB-Showcase`, `HyperLocal-App-Template` |
| Lists and data models | `quotes` (SQL + list), `bucketlist` (JSON file, multi-select, context actions), `vegetablesdatamodel`, `filtereddatamodel`, `pageddatamodel`, `datamanagerusage`, `listdecorators`, `ScrollableLists`, `repeater` |
| Settings, persistence | `starshipsettings`, `persistentobjects`, `jsonreadwrite`, `databasecreation`, `databasecrud` |
| Dialogs, toasts, prompts | `dialogs`, `DialogSample` |
| Notifications, LED, sound | `notifications`, `led`, `systemsound` |
| Active Frames | `cascadescookbookqml` (`MultiCover` + `SceneCover` from QML, `cover/AppCover.qml`), `HyperLocal-App-Template` (`activeFrameQML.cpp`) |
| Application menu (swipe down) | `cascadescookbookqml`, `BfB-Boilerplate-10.1`, `rundgang` (`Menu.definition`) |
| Invocation framework | `invoketarget` (be invoked), `invokeclient` (invoke others, queries), `socialinvocation`, `routemapinvoker`, `custombarcodeinvoker` |
| Headless (background) services | `headlesservice` + `headlesserviceui`, `pushCollectorHeadless` |
| Push | `pushCollector`, `pushCollectorHeadless` |
| Networking | `httptest`, `networking_ssl`, `networking_dwnld_file`, `imageloader` (threaded download + decode), `rssnews`, `weatherguesser`, `soapxml`, `mavenartifacts` (Qt 4.8 TLS 1.0 limits apply: see bb10-pitfalls) |
| Touch, gestures, animation | `pullmybeard`, `poemmaker`, `circularslider` (custom control), `imagerotation`, `xandos` |
| Native window in Cascades | `helloforeignwindow` (ForeignWindowControl; per-device QML in `mindw76h128du/`, `mindw90h160du/`, `mindw120h120du/`, `12ppd/`) |
| Keyboard-phone input | `gears` (Passport touch-keyboard swipes), `belt` (Classic belt keys and trackpad, 10.3.1), `highlighter` (10.3.1 effects and focus highlight for trackpad navigation) |
| Camera, media | `photobomber`, `appshot`, `dictaphone`, `nowplaying` |
| Sensors | `SensorDemo`, `compass`, `accelgame`, `tossgame`, `orientation` |
| BPS events into QML | `bpstoqml`, `batterysample` |
| Pickers, PIM | `filepicker`, `placepicker`, `calendar`, `messages`, `notebook` |
| Maps, location | `mapview`, `webmapview`, `locationdiagnostics` |
| Text input | `speedwriter` |
| Per-device assets | `stampcollector` (`assets/mindw120h120du/`), `HyperLocal-App-Template` (`720x720/`, `720x1280/`, `768x1280/`), `cascadescookbookqml` (`9ppd/`, `12ppd/`) |
| BBM, payments, NFC, Bluetooth | `bbmregistration`, `bbmprofile`, `bbmcontacts`, `paymentservice`, `nfcshare`, `nfcsender`, `nfcreceiver`, `bluetoothsppchat`, `bluetoothgatt` (BBM and payments relied on BlackBerry servers that are shut down) |
| Localisation | `IDS_QML_Sample`, `IDS_CPP_Sample` (`LocaleHandler`, `.qm` in `app/native/qm`) |

Quick searches:

```sh
# in a clone of the repo
grep -rn --include='*.qml' 'contextActions' . | head          # how a QML feature is used
grep -rln --include='bar-descriptor.xml' 'invoke-target' .     # descriptor features
```

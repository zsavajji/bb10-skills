# gate-app

A headless engine plus a one-page UI, used to test what needs an installed .bar (headless start, loopback link, TLS, speaker, now-playing). Copied from bbmusic (1d80813); not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| build-gate.sh | bbmusic, `scripts/build-gate.sh` (1d80813) | verbatim | Build the engine and the UI and package one unsigned .bar |
| gates/gate03.cpp | bbmusic, `tools/gates/gate03.cpp` (1d80813) | verbatim | Gate 0.3: TLS 1.3 to Google through libcurl and wolfSSL |
| gates/gate04.cpp | bbmusic, `tools/gates/gate04.cpp` (1d80813) | verbatim | Gate 0.4: WebM/Opus to the speaker through libasound |
| gates/gate05.cpp | bbmusic, `tools/gates/gate05.cpp` (1d80813) | verbatim | Gate 0.5: now-playing through its PPS objects with no Qt |
| app/assets/main.qml | bbmusic, `gateapp/app/assets/main.qml` (1d80813) | verbatim | The one-page UI that drives the gates |
| app/bar-descriptor.xml | bbmusic, `gateapp/app/bar-descriptor.xml` (1d80813) | verbatim | Bar descriptor of the gate app with the headless target |
| app/gate.pro | bbmusic, `gateapp/app/gate.pro` (1d80813) | verbatim | qmake project of the gate UI |
| app/src/EngineLink.cpp | bbmusic, `gateapp/app/src/EngineLink.cpp` (1d80813) | verbatim | Start the engine, read engine.json, connect, hello with the token |
| app/src/EngineLink.h | bbmusic, `gateapp/app/src/EngineLink.h` (1d80813) | verbatim | Start the engine, read engine.json, connect, hello with the token |
| app/src/main.cpp | bbmusic, `gateapp/app/src/main.cpp` (1d80813) | verbatim | UI entry point |
| app/src/NowPlaying.cpp | bbmusic, `gateapp/app/src/NowPlaying.cpp` (1d80813) | verbatim | Lock-screen controls with no MediaPlayer |
| app/src/NowPlaying.h | bbmusic, `gateapp/app/src/NowPlaying.h` (1d80813) | verbatim | Lock-screen controls with no MediaPlayer |
| app/src/UiLog.h | bbmusic, `gateapp/app/src/UiLog.h` (1d80813) | verbatim | UI log file in the data folder |
| engine/audio.cpp | bbmusic, `gateapp/engine/audio.cpp` (1d80813) | verbatim | Looping speaker thread from gate04 |
| engine/audio.h | bbmusic, `gateapp/engine/audio.h` (1d80813) | verbatim | Looping speaker thread from gate04 |
| engine/json.h | bbmusic, `gateapp/engine/json.h` (1d80813) | verbatim | Flat JSON messages of the gate protocol |
| engine/log.h | bbmusic, `gateapp/engine/log.h` (1d80813) | verbatim | Engine log and engine.json paths |
| engine/main.cpp | bbmusic, `gateapp/engine/main.cpp` (1d80813) | verbatim | Engine entry point: headless run, self test |
| engine/net.cpp | bbmusic, `gateapp/engine/net.cpp` (1d80813) | verbatim | libcurl and wolfSSL request from gate03 |
| engine/net.h | bbmusic, `gateapp/engine/net.h` (1d80813) | verbatim | libcurl and wolfSSL request from gate03 |
| engine/selftest.cpp | bbmusic, `gateapp/engine/selftest.cpp` (1d80813) | verbatim | Client that runs every command against the own server |
| engine/selftest.h | bbmusic, `gateapp/engine/selftest.h` (1d80813) | verbatim | Client that runs every command against the own server |
| engine/server.cpp | bbmusic, `gateapp/engine/server.cpp` (1d80813) | verbatim | Loopback TCP server with a token |
| engine/server.h | bbmusic, `gateapp/engine/server.h` (1d80813) | verbatim | Loopback TCP server with a token |
| engine/webm.h | bbmusic, `gateapp/engine/webm.h` (1d80813) | verbatim | WebM walk to get the Opus packets |

Left out of `app/assets/`: `test.webm` (a 680 KB test clip) and `cacert.pem` (Mozilla's CA bundle, fetched at build time: `curl -fsSL https://curl.se/ca/cacert.pem`). `main.qml` and the engine's audio code refer to them, so put both back before building the gate app.

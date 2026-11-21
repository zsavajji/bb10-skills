# third-party

Building C/C++ libraries for BB10. Copied from tengram (5a65cdf); not built in this repo.

| File | From | Kind | Shows |
|---|---|---|---|
| build-deps.sh, build-calls.sh, build-tgs.sh | `scripts/` | verbatim | Cross-building TDLib, calls libraries, rlottie |
| tgs.map | `tests/tgs.map` | verbatim | Symbol map for the tgs library |
| fetch-libwebp.sh, fetch-qrcodegen.sh, fetch-qtsqlcipher.sh, fetch-emoji.sh | `scripts/` | verbatim | Pinned downloads |
| patches/td/*.patch | `patches/td/` | verbatim | TDLib patches for QNX |
| aeabi_mem.c | `ui/sqlcipher/` | verbatim | ARM run-time helpers clang calls |
| tdsqlite_names.h | `ui/sqlcipher/` | verbatim | Maps SQLite names to the td-prefixed ones |
| sqlcipher-test.cpp | `ui/sqlcipher/test.cpp` | verbatim | Read the encrypted db through QtSql |
| sqlcipher-test.pro | `ui/sqlcipher/test.pro` | verbatim | qmake project of that test |
| build-qtsqlcipher-test.sh | `scripts/` | verbatim | Build the SQLCipher Qt SQL driver test |
| build-emoji-font.py | `scripts/` | verbatim | Derive a small emoji font from Noto Color Emoji |
| tgs_check.c | `tests/` | verbatim | Can the phone play animated stickers |
| calls/CMakeLists.txt | `calls/` | verbatim | tg_owt and tgcalls as static libraries |
| calls/QnxInterface.cpp | `calls/` | verbatim | tgcalls platform layer for BB10, video passed through |
| calls/QnxVideo.h | `calls/` | verbatim | Video interface the engine sees |
| patches/tg_owt/0001-qnx-port.patch | `patches/tg_owt` | verbatim | QNX port of tg_owt |
| patches/tgcalls/0001-qnx-no-transport-cc.patch | `patches/tgcalls` | verbatim | tgcalls without transport-cc |

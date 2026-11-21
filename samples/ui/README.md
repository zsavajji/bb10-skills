# samples/ui

These samples are not built in this repo: they are reference code copied from the app repos.

| File | From | Kind | Shows |
|---|---|---|---|
| app.pro | phoenix app/phoenix.pro @ 4e06969 | verbatim | qmake project of a Cascades UI app |
| app-gcc483.pro | tengram app/tengram.pro @ 5a65cdf | verbatim | gcc 4.8.3 build, C libraries, lupdate_only |
| main.cpp | phoenix app/src/main.cpp @ 4e06969 | verbatim | codecs, checkDocuments, root context |
| main-logging.cpp | hyperberry app/src/main.cpp @ df45afc | excerpt | qInstallMsgHandler to slog2 and a file, start time, QML errors file |
| UiLog.h | phoenix app/src/UiLog.h @ 4e06969 | verbatim | UI log helper |
| scripts/build-app.sh | phoenix scripts/build-app.sh @ 4e06969 | adapted | UI build; LINT points at skills/bb10-build-deploy/scripts/cascades-qmllint.sh (marked) |
| scripts/build-ui-checks.sh | tengram scripts/build-ui.sh @ 5a65cdf | verbatim | UI build with checks |
| scripts/gen-icons.py | phoenix scripts/gen-icons.py @ 4e06969 | verbatim | icon generation |
| scripts/gen-app-icon.py | tengram scripts/gen-icon.py @ 5a65cdf | verbatim | app icon generation |
| bbmusic.pro | bbmusic app/ @ 1d80813 | verbatim | qmake project of a UI beside a headless engine |
| hyperberry.pro | hyperberry app/ @ df45afc | verbatim | qmake project of the UI of a headless engine app |
| errors.js | hyperberry app/assets/ @ df45afc | verbatim | Failed request code to user text |
| tests/format_test.js | phoenix app/tests/ @ 4e06969 | verbatim | Node test of the pages pure helpers |
| tests/queue_test.js | bbmusic app/tests/ @ 1d80813 | verbatim | Node test of queue.js |
| tests/test-phone.js | hyperberry scripts/ @ df45afc | verbatim | Node test of a QML-side JS file |
| scripts/build-app-bbmusic.sh | bbmusic scripts/build-app.sh @ 1d80813 | verbatim | UI plus engine build and unsigned package |
| scripts/build-app-hyperberry.sh | hyperberry scripts/build-app.sh @ df45afc | adapted | UI plus engine build, memory addon and helper; RLIMIT_PATCH replaces a sibling path (marked) |
| scripts/build-app-tengram.sh | tengram scripts/build-app.sh @ 5a65cdf | adapted | UI plus engine build, memory addon and helper; RLIMIT_PATCH replaces a sibling path (marked) |
| scripts/gen-icons-bbmusic.py | bbmusic scripts/gen-icons.py @ 1d80813 | verbatim | Icons drawn with zlib only |
| scripts/gen-icons-hyperberry.py | hyperberry scripts/gen-icons.py @ df45afc | verbatim | Chat marks and action icons drawn in code |
| scripts/gen-app-icon-phoenix.py | phoenix scripts/gen-app-icon.py @ 4e06969 | verbatim | Launcher icon and splash screens from an SVG glyph |

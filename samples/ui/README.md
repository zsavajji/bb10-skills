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

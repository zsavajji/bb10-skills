# Phoenix for Reddit's UI, a plain executable as the bar's Qnx/Elf entry point. Built out of tree
# by scripts/build-app.sh (build/app-ui).
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

# The .ts files: scripts/build-app.sh updates them from the QML and compiles them to build/qm.
TRANSLATIONS += translations/phoenix_it.ts translations/phoenix_fr.ts translations/phoenix_de.ts \
    translations/phoenix_pl.ts translations/phoenix_ru.ts translations/phoenix_hi.ts \
    translations/phoenix_vi.ts translations/phoenix_id.ts

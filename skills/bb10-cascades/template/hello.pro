# A plain executable as the entry point (the bar's Qnx/Elf entry). The IDE's
# cascades10 config instead builds a release as a .so for the Cascades
# launcher (entryPointType Qnx/Cascades); either works, this one is simpler,
# and about 3 s slower at every start: docs/14-cascades-launcher.md has the
# three changes (TEMPLATE = lib, the descriptor, no applicationDirPath()).
TEMPLATE = app
TARGET = hello
CONFIG += qt warn_on release
QT += declarative network
# No QtGui: use bb::ImageData / bb::utility::ImageConverter instead of QImage.
QT -= gui
LIBS += -lbbcascades -lbbdata -lbbsystem -lbb -lbbutility
QMAKE_LFLAGS += -Wl,-rpath-link,$${QNX_DIR}/$${QNX_CPUDIR}/usr/lib/qt4/lib
QMAKE_CXXFLAGS += -Wno-psabi

SOURCES += src/main.cpp src/backend.cpp
HEADERS += src/backend.h

DESTDIR = app
OBJECTS_DIR = obj
MOC_DIR = moc

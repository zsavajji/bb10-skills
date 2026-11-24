// base-bb10: an excerpt of tengram app/src/main.cpp @ dbe35bf: what a UI started through the Cascades launcher
// (docs/14-cascades-launcher.md) needs of main.cpp. Cuts are marked "[...] app logic left out".
#include <cstring>
#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/platform/NotificationDefaultApplicationSettings>
#include <bb/system/LocaleHandler>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QTimer>
#include <QTranslator>
#include <QtDeclarative>
#include <cstdio>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/slog2.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// [...] app logic left out: the app's own headers, the log (samples/ui/main-logging.cpp)

using namespace bb::cascades;

// Milliseconds since the system started this process, -1 when it won't say. With "ui: main" and "ui: scene
// set" it times a cold start. As a plain executable main came 1.5 s in and the application was made by 3.4 s, the
// system's share (103 libraries, 97 of them Cascades' own, then the Application's own file lookups), and the scene
// was set at 4.35 s with 500 chats (Classic, 1.0.2.128). Forked from the launcher's /bin/cascades, which holds all
// that already: 0.14 s, 0.29 s and 1.47 s (1.0.2.129, the engine up).
static int sinceLaunch()
{
    procfs_info info;
    const int fd = open("/proc/self/as", O_RDONLY);
    if (fd < 0)
        return -1;
    const int rc = devctl(fd, DCMD_PROC_INFO, &info, sizeof info, 0);
    close(fd);
    if (rc != EOK)
        return -1;
    timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return int((qint64(now.tv_sec) * 1000000000LL + now.tv_nsec - qint64(info.start_time)) / 1000000);
}

// [...] app logic left out: the QML error file

Q_DECL_EXPORT int main(int argc, char **argv)
{
    const int atMain = sinceLaunch();
    Application app(argc, argv);
    startLog();
    qDebug("ui: main %d ms after launch, the application made at %d ms", atMain, sinceLaunch());
    // The sources are UTF-8; Qt 4 reads tr() and C-string literals as Latin-1 unless told, and
    // "typing…" showed as "typingà|" (Q10, 2026-10-31).
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    // The phone's language, once at start (no live switching): the .qm is in the bar's assets, and
    // English, the source text, has none. Before the QML loads, whose qsTr reads it then. Not applicationDirPath():
    // that is the launcher's /bin now; the bar's folder is app/native under the working directory.
    QTranslator translator;
    const QString language = bb::system::LocaleHandler(bb::system::LocaleType::Messages).locale().name().section('_', 0, 0);
    if (translator.load("tengram_" + language, QDir::currentPath() + "/app/native/assets/translations"))
        app.installTranslator(&translator);
    // [...] app logic left out: the engine link, the models, the QML types, main.qml and its context
    app.setScene(root);
    const int sceneAt = sinceLaunch();
    qDebug("ui: scene set at %d ms", sceneAt);
    // [...] app logic left out: the address book's first read
    return Application::exec();
}

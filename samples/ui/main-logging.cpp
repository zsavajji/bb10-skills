// Excerpt of hyperberry/app/src/main.cpp: the log handler (qInstallMsgHandler to slog2 and a file),
// the start-time probe and the QML-errors file. Includes trimmed to those the excerpt uses.
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtDeclarative>
#include <cstdio>
#include <devctl.h>
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/slog2.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

using namespace bb::cascades;

// qDebug and qWarning, QML's errors included, go nowhere on the phone by themselves: they go to
// the system log, where `slog2info -b hyperberry` shows them, and to data/logs/ui.log. The file
// outlives the process: the system recycles a closed app's log within minutes, and a freeze on
// 0.0.1.132 left nothing to read (Q10, 2026-11-02).
static slog2_buffer_t logBuffer;
static bool toSlog = false;
static FILE *logFile = 0;

static void writeLog(QtMsgType type, const char *msg)
{
    if (toSlog)
        slog2c(logBuffer, 0, type == QtDebugMsg ? SLOG2_INFO : SLOG2_WARNING, msg);
    if (!logFile)
        return;
    timeval now;
    gettimeofday(&now, 0);
    tm local;
    localtime_r(&now.tv_sec, &local);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%b %d %H:%M:%S", &local); // slog2info's form, phone time
    fprintf(logFile, "%s.%03d %s\n", stamp, int(now.tv_usec / 1000), msg);
    fflush(logFile); // with the OS at once: a freeze or a kill keeps every line before it
}

static void startLog()
{
    slog2_buffer_set_config_t config = {};
    config.num_buffers = 1;
    config.buffer_set_name = "hyperberry";
    config.verbosity_level = SLOG2_INFO;
    config.buffer_config[0].buffer_name = "ui";
    config.buffer_config[0].num_pages = 4;
    toSlog = slog2_register(&config, &logBuffer, 0) == 0;
    const QString path = QDir::homePath() + "/logs/ui.log";
    QDir().mkpath(QFileInfo(path).path());
    // ponytail: rotated at start only, so one long run can pass 256 KB; the UI writes little.
    if (QFileInfo(path).size() > 256 * 1024) {
        QFile::remove(path + ".1");
        QFile::rename(path, path + ".1");
    }
    logFile = fopen(QFile::encodeName(path).constData(), "a");
    qInstallMsgHandler(writeLog);
    qDebug("UI started, pid %d", int(getpid()));
}

// Milliseconds since the system started this process, -1 when it won't say. With "ui: main" and "ui: scene
// set" it times a cold start: on the Q10 main comes 1.2 s in and the application is made by 1.6 s, the
// system's share (103 libraries, 97 of them Cascades' own), and the scene is set at 2.2 s (0.1.1.310; 3.2 s
// before the speed work of 2026-11-13).
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

// A QML error otherwise quits the app at start with nothing to show: write it where the user
// can find it (File Manager > misc, or the app's data folder).
static bool loaded(QmlDocument *document, const char *name)
{
    if (!document->hasErrors())
        return true;
    QFileInfo misc("/accounts/1000/shared/misc");
    QFile log((misc.isDir() && misc.isWritable() ? misc.filePath() : QDir::homePath())
              + "/hyperberry-qml-errors.log");
    if (log.open(QIODevice::WriteOnly | QIODevice::Append)) {
        foreach (const QDeclarativeError &error, document->errors())
            log.write(QString("%1: %2\n").arg(name, error.toString()).toUtf8());
    }
    return false;
}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    const int atMain = sinceLaunch();
    Application app(argc, argv);
    startLog();
    qDebug("ui: main %d ms after launch, the application made at %d ms", atMain, sinceLaunch());
    // [...] app logic left out: translator, EngineLink, models, context properties, QML load
    app.setScene(root);
    qDebug("ui: scene set at %d ms", sinceLaunch());
    return Application::exec();
}

#include "EngineLink.h"

#include <bb/data/JsonDataAccess>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <QDir>
#include <QRegExp>
#include <QStringList>

#include "UiLog.h"

using namespace bb::data;

static const int QuickMs = 5000; // quick attempts for this long after a start or a drop
static const int RetryMs = 300;
static const int SlowMs = 5000;
static const int DownMs = 10000; // no link for this long: the engine is down, not waking

// JS numbers can reach C++ as doubles: whole ones must go out as 1, not 1.0.
static QVariant normalized(const QVariant &value)
{
    if (value.type() == QVariant::Double) {
        const double d = value.toDouble();
        if (d == floor(d) && fabs(d) < 2147483647.0)
            return QVariant(int(d));
    } else if (value.type() == QVariant::Map) {
        const QVariantMap in = value.toMap();
        QVariantMap out;
        for (QVariantMap::const_iterator i = in.constBegin(); i != in.constEnd(); ++i)
            out.insert(i.key(), normalized(i.value()));
        return out;
    } else if (value.type() == QVariant::List) {
        const QVariantList in = value.toList();
        QVariantList out;
        for (int i = 0; i < in.size(); ++i)
            out.append(normalized(in.at(i)));
        return out;
    }
    return value;
}

// A line for ui.log: its beginning, with the API key masked (the user's own secret; the log is
// read over ssh and pasted around).
static QString forLog(const QByteArray &line)
{
    QString text = QString::fromUtf8(line.left(220));
    text.replace(QRegExp("(\"client_id\"\\s*:\\s*\")[^\"]*"), "\\1***");
    return text;
}

EngineLink::EngineLink(QObject *parent)
    : QObject(parent)
    , m_proc(0)
    , m_connected(false)
    , m_down(false)
    , m_seq(0)
{
    // Qt 4.8's QElapsedTimer holds garbage until started or invalidated.
    m_began.invalidate();
    m_retry.setSingleShot(true);
    connect(&m_retry, SIGNAL(timeout()), this, SLOT(attempt()));
    m_downTimer.setSingleShot(true);
    connect(&m_downTimer, SIGNAL(timeout()), this, SLOT(wentDown()));
}

EngineLink::~EngineLink()
{
    if (m_proc) {
        m_proc->disconnect(this);
        if (m_proc->state() != QProcess::NotRunning) {
            // The engine exits when its stdin closes; kill it if it does not.
            m_proc->closeWriteChannel();
            if (!m_proc->waitForFinished(500)) {
                m_proc->kill();
                m_proc->waitForFinished(500);
            }
        }
    }
}

void EngineLink::start()
{
    if (m_connected)
        return;
    m_began.start();
    if (!m_down && !m_downTimer.isActive())
        m_downTimer.start(DownMs); // from the launch; a Retry does not start it again
    attempt();
}

void EngineLink::retry()
{
    if (m_connected)
        return;
    m_retry.stop();
    start();
}

void EngineLink::scheduleAttempt()
{
    m_retry.start(m_began.isValid() && m_began.elapsed() > QuickMs ? SlowMs : RetryMs);
}

void EngineLink::attempt()
{
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        return;
    if (m_proc) {
        m_proc->disconnect(this);
        m_proc->deleteLater();
        m_proc = 0;
    }
    // Cascades leaves its navigator, GPU and screen descriptors open across exec: without this
    // the engine would inherit them. QProcess makes its own pipes after this, unaffected.
    for (long fd = 3, max = sysconf(_SC_OPEN_MAX); fd < max; ++fd)
        fcntl((int)fd, F_SETFD, FD_CLOEXEC);

    const QString home = QDir::homePath();
    const QString program = QDir::currentPath() + "/app/native/phoenixd";
    m_proc = new QProcess(this);
    m_proc->setStandardErrorFile(home + "/phoenixd.stderr", QIODevice::Append);
    connect(m_proc, SIGNAL(started()), this, SLOT(childStarted()));
    connect(m_proc, SIGNAL(readyReadStandardOutput()), this, SLOT(readLines()));
    connect(m_proc, SIGNAL(finished(int, QProcess::ExitStatus)),
            this, SLOT(childFinished(int, QProcess::ExitStatus)));
    connect(m_proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(childError(QProcess::ProcessError)));
    uiLog("engine start: " + program + " --data " + home);
    m_proc->start(program, QStringList() << "--data" << home);
}

void EngineLink::childStarted()
{
    uiLog("engine started");
    QByteArray buffer;
    m_statusId = QString::number(++m_seq);
    JsonDataAccess json;
    QVariantMap message;
    message.insert("@", m_statusId);
    message.insert("cmd", "status");
    json.saveToBuffer(QVariant(message), &buffer);
    buffer.replace('\n', "").replace('\r', "");
    writeLine(buffer);
}

void EngineLink::childFinished(int exitCode, QProcess::ExitStatus status)
{
    readLines(); // what it said last
    uiLog(QString("engine exited: %1, code %2")
              .arg(status == QProcess::CrashExit ? "crashed" : "normal exit").arg(exitCode));
    lost();
}

void EngineLink::childError(QProcess::ProcessError error)
{
    uiLog(QString("engine error %1: %2").arg(int(error)).arg(m_proc ? m_proc->errorString() : QString()));
    // A crash is followed by finished(); a start that fails is not.
    if (error == QProcess::FailedToStart)
        lost();
}

void EngineLink::lost()
{
    const bool was = m_connected;
    m_connected = false;
    m_statusId.clear();
    if (was) {
        m_began.start(); // quick attempts again
        emit connectedChanged();
    }
    if (!m_down && !m_downTimer.isActive())
        m_downTimer.start(DownMs);
    // Whoever waits for an answer would wait for ever (a page's spinner, its request id).
    const QSet<QString> unanswered = m_pending;
    m_pending.clear();
    foreach (const QString &id, unanswered)
        emit reply(id, false, QVariant(), "disconnected", "The engine is not connected.");
    scheduleAttempt();
}

void EngineLink::wentDown()
{
    m_down = true;
    uiLog("engine not up after 10 s");
    emit downChanged();
}

void EngineLink::readLines()
{
    if (!m_proc)
        return;
    // A line can be hundreds of kB and arrive in pieces: canReadLine() is true only for a whole one.
    while (m_proc->canReadLine()) {
        const QByteArray line = m_proc->readLine().trimmed();
        if (!line.isEmpty())
            handleLine(line);
    }
}

void EngineLink::handleLine(const QByteArray &line)
{
    JsonDataAccess json;
    const QVariant doc = json.loadFromBuffer(line);
    if (json.hasError() || doc.type() != QVariant::Map) {
        uiLog("< not JSON: " + QString::fromUtf8(line.left(120)));
        return;
    }
    QVariantMap map = doc.toMap();
    const QString id = map.value("@").toString();
    if (map.value("ev").toString() != "thumb") // one per row of a list
        uiLog("< " + forLog(line));
    if (map.contains("ev")) {
        const QString name = map.take("ev").toString();
        emit event(name, map);
        return;
    }
    const bool ok = map["ok"].toBool();
    const QVariantMap error = map["error"].toMap();
    const QVariant result = map["result"];
    if (!m_statusId.isEmpty() && id == m_statusId) {
        m_statusId.clear();
        if (!ok) {
            uiLog("status refused");
            return;
        }
        m_connected = true;
        uiLog("connected");
        m_downTimer.stop();
        if (m_down) {
            m_down = false;
            emit downChanged();
        }
        emit connectedChanged();
        emit event("status", result.toMap());
        return;
    }
    m_pending.remove(id);
    emit reply(id, ok, result, error["code"].toString(), error["message"].toString());
}

void EngineLink::writeLine(const QByteArray &json)
{
    uiLog("> " + forLog(json));
    m_proc->write(json + "\n");
}

QString EngineLink::request(const QString &cmd, const QVariantMap &fields)
{
    if (!m_connected || !m_proc || m_proc->state() != QProcess::Running)
        return QString();
    const QString id = QString::number(++m_seq);
    QVariantMap message = normalized(fields).toMap();
    message.insert("@", id);
    message.insert("cmd", cmd);
    QByteArray buffer;
    JsonDataAccess json;
    json.saveToBuffer(QVariant(message), &buffer);
    // JsonDataAccess may indent its output; inside strings newlines are escaped, so these are layout.
    buffer.replace('\n', "").replace('\r', "");
    writeLine(buffer);
    m_pending.insert(id);
    return id;
}

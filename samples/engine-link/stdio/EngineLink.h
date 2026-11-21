#ifndef ENGINELINK_H
#define ENGINELINK_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>

// The UI's side of the engine protocol: starts phoenixd as a child process (app/native/phoenixd
// --data <home>) and speaks NDJSON over its stdin/stdout. When the child dies or does not start
// it starts it again: every 300 ms for 5 s, then every 5 s. The engine exits when its stdin closes.
class EngineLink : public QObject
{
    Q_OBJECT
    // True once the child runs and has answered a status request.
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    // True once there has been no link for 10 s straight: the UI says so then, not while the
    // engine is still waking.
    Q_PROPERTY(bool down READ down NOTIFY downChanged)
public:
    explicit EngineLink(QObject *parent = 0);
    ~EngineLink();

    bool connected() const { return m_connected; }
    bool down() const { return m_down; }

    // Begins starting the engine (and keeps at it).
    Q_INVOKABLE void start();
    // The "Retry" button: a new round of quick attempts now.
    Q_INVOKABLE void retry();
    // Sends {"@":<fresh id>,"cmd":cmd,...fields} and returns the id; "" when not connected.
    // Every id gets one reply(): the engine's, or, if the child dies first, a failure with the
    // code "disconnected".
    Q_INVOKABLE QString request(const QString &cmd, const QVariantMap &fields);

signals:
    void connectedChanged();
    void downChanged();
    void reply(QString id, bool ok, QVariant result, QString errorCode, QString errorMessage);
    // The engine's events (lines with "ev"). Also one made-up: "status" (fields = the status
    // result), after the connection is up.
    void event(QString name, QVariantMap fields);

private slots:
    void attempt();
    void readLines();
    void childStarted();
    void childFinished(int exitCode, QProcess::ExitStatus status);
    void childError(QProcess::ProcessError error);
    void wentDown();

private:
    void lost();
    void writeLine(const QByteArray &json);
    void handleLine(const QByteArray &line);
    void scheduleAttempt();

    QProcess *m_proc;
    QTimer m_retry;
    QTimer m_downTimer;
    QElapsedTimer m_began;
    bool m_connected;
    bool m_down;
    int m_seq;
    QString m_statusId;
    QSet<QString> m_pending; // requests sent and not answered yet
};

#endif

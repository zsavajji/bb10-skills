#ifndef UILOG_H
#define UILOG_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QString>

// Appends a timestamped line to ui.log in the app's data folder, so the UI's side (QML errors,
// the engine link's state) can be read after the app is gone. A file past 512 kB starts afresh.
inline void uiLog(const QString &text)
{
    QFile file(QDir::homePath() + "/ui.log");
    if (file.size() > 512 * 1024)
        file.remove();
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        file.write((QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") + text + "\n").toUtf8());
        file.close();
    }
}

#endif

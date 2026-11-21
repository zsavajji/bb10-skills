#include "backend.h"

#include <QDir>
#include <bb/system/Clipboard>

Backend::Backend(QObject *parent)
    : QObject(parent)
    , m_settings(QDir::homePath() + "/settings.ini", QSettings::IniFormat)
{
    m_notes = m_settings.value("notes").toStringList();
}

QVariantList Backend::notes() const
{
    QVariantList rows;
    for (int i = 0; i < m_notes.size(); ++i) {
        QVariantMap row;
        row["text"] = m_notes.at(i);
        row["index"] = i;
        rows << row;
    }
    return rows;
}

void Backend::addNote(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    m_notes << text.trimmed();
    // BB10 may kill a backgrounded app without a clean exit: save now.
    m_settings.setValue("notes", m_notes);
    m_settings.sync();
    emit notesChanged();
}

void Backend::removeNote(int index)
{
    if (index < 0 || index >= m_notes.size())
        return;
    m_notes.removeAt(index);
    m_settings.setValue("notes", m_notes);
    m_settings.sync();
    emit notesChanged();
}

void Backend::copy(const QString &text)
{
    bb::system::Clipboard clipboard;
    clipboard.clear();
    clipboard.insert("text/plain", text.toUtf8());
    emit message(tr("Copied"));
}

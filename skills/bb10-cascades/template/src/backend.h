#ifndef BACKEND_H
#define BACKEND_H

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariant>

// What QML sees as `backend`: properties with NOTIFY signals for bindings,
// Q_INVOKABLE methods to call. Qt 4.8: SIGNAL/SLOT macros, no lambdas.
class Backend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QString lastNote READ lastNote NOTIFY notesChanged)

public:
    explicit Backend(QObject *parent = 0);

    QVariantList notes() const;
    QString lastNote() const { return m_notes.isEmpty() ? QString() : m_notes.last(); }

    Q_INVOKABLE void addNote(const QString &text);
    Q_INVOKABLE void removeNote(int index);
    Q_INVOKABLE void copy(const QString &text);

signals:
    void notesChanged();
    void message(const QString &text);

private:
    QSettings m_settings;   // data/settings.ini in the app's sandbox
    QStringList m_notes;
};

#endif

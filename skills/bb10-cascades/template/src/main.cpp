#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/Container>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/SceneCover>
#include <QDir>
#include <QTextCodec>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QtDeclarative>

#include "backend.h"

using namespace bb::cascades;

// A QML error otherwise quits the app at start with nothing to show: write it
// where the user can find it (File Manager > misc, or the app's data folder).
static bool loaded(QmlDocument *document, const char *name)
{
    if (!document->hasErrors())
        return true;
    QFileInfo misc("/accounts/1000/shared/misc");
    QFile log((misc.isDir() && misc.isWritable() ? misc.filePath() : QDir::homePath())
              + "/hello-qml-errors.log");
    if (log.open(QIODevice::WriteOnly | QIODevice::Append)) {
        foreach (const QDeclarativeError &error, document->errors())
            log.write(QString("%1: %2\n").arg(name, error.toString()).toUtf8());
    }
    return false;
}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    // Qt 4 reads tr() and C string literals as Latin-1; the sources are UTF-8.
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    Backend backend(&app);

    // Cascades has no Timer; Qt's is registered before any QML is created.
    qmlRegisterType<QTimer>("hello", 1, 0, "Timer");

    QmlDocument *document = QmlDocument::create("asset:///main.qml").parent(&app);
    if (!loaded(document, "main.qml"))
        return 1;
    // Only the root page reads `backend` here. If list rows must read an app object, set it on
    // QmlDocument::defaultDeclarativeEngine()->rootContext() instead: a list item's context does
    // not descend from the document's (skills: bb10-cascades/references/writing-notes.md).
    document->setContextProperty("backend", &backend);
    AbstractPane *root = document->createRootObject<AbstractPane>();
    if (!root)
        return 1;
    app.setScene(root);

    // The Active Frame shown while the app is minimized, from its own document.
    // (QML can do it too: a SceneCover in attachedObjects and
    // Application.setCover(it), as cascadescookbookqml does with MultiCover.)
    QmlDocument *coverDocument = QmlDocument::create("asset:///cover.qml").parent(&app);
    if (loaded(coverDocument, "cover.qml")) {
        coverDocument->setContextProperty("backend", &backend);
        if (Container *content = coverDocument->createRootObject<Container>()) {
            SceneCover *cover = new SceneCover(&app);
            cover->setContent(content);
            app.setCover(cover);
        }
    }
    return Application::exec();
}

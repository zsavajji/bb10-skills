#include <bb/cascades/AbstractPane>
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/ApplicationInfo>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QStringList>
#include <QTextCodec>
#include <QTimer>
#include <QTranslator>
#include <QtDeclarative>

#include "EngineLink.h"
#include "Opener.h"
#include "Prefs.h"
#include "Router.h"
#include "Session.h"
#include "Thumbs.h"
#include "UiLog.h"

using namespace bb::cascades;

// When data/check-qml exists (put there by hand over ssh), every QML document of the app is
// compiled at start and what the compiler says goes to ui.log: "check: ok X.qml" or the
// errors. Most pages are created only when first opened, so without this a fault in one shows
// only as a page that never appears. It finds what the compiler finds (unknown types,
// properties, signal handlers, enum values, JavaScript that does not parse), not what a binding
// does when it runs. It needs the running app: QmlDocument aborts without a
// bb::cascades::Application, and that cannot be built from a shell (both tried, 2026-11-14).
static void checkDocuments(Application &app)
{
    if (!QFile::exists(QDir::homePath() + "/check-qml"))
        return;
    const QStringList names = QDir("app/native/assets").entryList(QStringList("*.qml"), QDir::Files, QDir::Name);
    int failed = 0;
    foreach (const QString &name, names) {
        QmlDocument *document = QmlDocument::create("asset:///" + name).parent(&app);
        if (!document->hasErrors()) {
            uiLog("check: ok " + name);
        } else {
            ++failed;
            foreach (const QDeclarativeError &error, document->errors())
                uiLog("check: FAIL " + name + ": " + error.toString());
        }
        document->deleteLater();
    }
    uiLog(QString("check: %1 of %2 documents failed").arg(failed).arg(names.size()));
}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    // tr() and C strings read as UTF-8 (Qt 4 takes them as Latin-1).
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    uiLog("ui started");

    // The phone's language, when the app has it (app/translations: it fr de pl ru hi vi id);
    // English otherwise, which is what the sources are written in.
    // ponytail: read once at start; a change in the system's settings shows on the next launch.
    QTranslator translator;
    if (translator.load("phoenix_" + QLocale().name(), "app/native/qm"))
        app.installTranslator(&translator);

    EngineLink engine(&app);
    Thumbs thumbs(&engine, &app);
    Prefs prefs(&app);
    Opener opener(&app);
    Session session(&engine, &app);
    Router router(&app);

    // bb.cascades has no Timer.
    qmlRegisterType<QTimer>("phoenix", 1, 0, "Timer");
    QmlDocument *document = QmlDocument::create("asset:///main.qml").parent(&app);
    if (document->hasErrors()) {
        // A QML error otherwise quits the app at start with nothing to show.
        foreach (const QDeclarativeError &error, document->errors())
            uiLog("QML: " + error.toString());
        return 1;
    }
    // On the engine's root context, not the document's: a list item is made in a context of its
    // own, which the document's context properties do not reach. On the phone (0.1.0.14,
    // 2026-11-21) every row's `prefs.textScale` came out as nothing and the text was drawn at
    // size 0. Every context descends from the root one, so the rows see these too.
    QDeclarativeContext *shared = QmlDocument::defaultDeclarativeEngine()->rootContext();
    shared->setContextProperty("engine", &engine);
    shared->setContextProperty("thumbs", &thumbs);
    shared->setContextProperty("prefs", &prefs);
    shared->setContextProperty("opener", &opener);
    shared->setContextProperty("session", &session);
    shared->setContextProperty("router", &router);
    // The package's version, for About.
    shared->setContextProperty("appVersion", bb::ApplicationInfo().version());
    AbstractPane *root = document->createRootObject<AbstractPane>();
    if (!root) {
        uiLog("QML: no root object");
        return 1;
    }
    root->setParent(document);
    app.setScene(root);
    checkDocuments(app);
    engine.start();
    const int rc = Application::exec();
    app.setScene(0);
    return rc;
}

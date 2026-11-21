# Cookbook snippets (excerpts from the Tengram UI)

Short excerpts; each is the whole technique, the rest of the function is left out.

## Clipboard (ChatController.cpp)

```cpp
void ChatController::copy(const QString &text)
{
    bb::system::Clipboard clipboard;
    clipboard.clear();
    clipboard.insert("text/plain", text.toUtf8());
}
```

## Lifecycle: thumbnail and fullscreen (EngineLink.cpp)

```cpp

void EngineLink::start()
{
    // Each of these can fire twice for one change; setFullscreen acts only on a change.
    bb::cascades::Application *app = bb::cascades::Application::instance();
    connect(app, SIGNAL(fullscreen()), this, SLOT(onFullscreen()));
    connect(app, SIGNAL(thumbnail()), this, SLOT(onNotFullscreen()));
    connect(app, SIGNAL(invisible()), this, SLOT(onNotFullscreen()));
    tryConnect();
```

```cpp
void EngineLink::onFullscreen()
{
    setFullscreen(true);
    m_retryDelay = RetryMinMs;
    if (m_retry.isActive()) {
        m_retry.stop();
        tryConnect();
    }
}

void EngineLink::onNotFullscreen() { setFullscreen(false); }


void EngineLink::setFullscreen(bool on)
{
    if (on == m_fullscreen)
        return;
    beat(); // a stall that ends with leaving the app is still told
    m_fullscreen = on;
    if (on) {
        m_beat.start(BeatMs);
        m_beatAt.restart();
    } else {
        m_beat.stop();
    }
    sendUiState();
    emit fullscreenChanged();
}
```

## Version and screen size (main.cpp)

```cpp
    // About shows it where a profile shows the number: versionNumber.buildId, from the .bar.
    document->documentContext()->setContextProperty("appVersion", bb::ApplicationInfo().version());
    // The pages' px(): a 720 px stage scaled to the screen's width.
    document->documentContext()->setContextProperty("screenWidth", bb::device::DisplayInfo().pixelSize().width());
    // How far a tall screen (the Z10's 1280) runs above that stage.
    document->documentContext()->setContextProperty("screenHeight", bb::device::DisplayInfo().pixelSize().height());
```

## Shared files (MediaOpener.cpp)

```cpp
// A received photo lives in the sandbox (phase6.md, choice 1), where neither Pictures nor File
// Manager looks: Save puts a copy where both do. A copy already there is the answer.
QString MediaOpener::saveToPictures(const QString &path)
{
    const QString dir = "/accounts/1000/shared/photos/Tengram";
    if (!QDir().mkpath(dir))
        return QString();
    const QString target = dir + "/" + QFileInfo(path).fileName();
    if (QFile::exists(target) || QFile::copy(path, target))
        return target;
    return QString();
}
```

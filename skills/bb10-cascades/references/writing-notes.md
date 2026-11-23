# How to write it: findings from four apps

Read when: you write `main.cpp`, swap the scene, animate a page, expose C++ to QML, or draw list rows in C++. Each entry has its source
and an evidence tag. Tags as in **bb10-pitfalls**: [Q10], [Classic], [SDK], [app] (one of our four apps, or a third-party one),
[untested]. "Seen on the phone" is written where the source says so. The apps: HyperBerry (WhatsApp, [hyperberry](https://github.com/zsavajji/hyperberry)), Tengram
(Telegram, [tengram](https://github.com/zsavajji/tengram)), BBMusic ([bbmusic](https://github.com/zsavajji/bbytmusic)), Phoenix (Reddit, [phoenix](https://github.com/zsavajji/phoenix-reddit)). Build-side facts (`.pro`, codecs, build
scripts) are in `../../../docs/07-ui-build.md`; this file is about the code.

## 1. `main.cpp`

Shape that all four apps share (`../../../docs/07-ui-build.md` section 5):

```cpp
Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));        // tr() reads Latin-1 otherwise
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
    qmlRegisterType<QTimer>("myapp", 1, 0, "Timer");                     // before QmlDocument::create
    QmlDocument *document = QmlDocument::create("asset:///main.qml").parent(&app);
    if (document->hasErrors()) { /* log document->errors(); */ return 1; }
    /* context properties: see 1.2 */
    AbstractPane *root = document->createRootObject<AbstractPane>();
    if (!root) return 1;
    root->setParent(document);       // see 1.3
    app.setScene(root);
    engine.start();                  // last, after the scene is set
    const int rc = Application::exec();
    app.setScene(0);                 // before the objects it holds go away
    return rc;
}
```

**1.1 UTF-8 codecs.** Qt 4 reads `tr()` and C string literals as Latin-1: Tengram's "typing..." showed as "typingà|" [Q10, 2026-10-31].
`setCodecForTr` and `setCodecForCStrings` to UTF-8 fix it; `QTextStream` also needs `setCodec("UTF-8")` for non-ASCII. [Q10] (`tengram` and
`hyperberry` `main.cpp`; BBMusic's does not set them: a new app should.) Set `CODECFORTR`/`CODECFORSRC = UTF-8` in the `.pro` for `lupdate`.

**1.2 Context properties on the root context.** `QmlDocument::setContextProperty` sets them on the document's context, which a list item's
context does not reach: in Phoenix 0.1.0.14 (2026-11-21) every row's `prefs.textScale` came out as nothing and the text was drawn at size 0 [app,
seen on the phone]. Set them on the engine's root context, from which every context descends:

```cpp
QDeclarativeContext *shared = QmlDocument::defaultDeclarativeEngine()->rootContext();
shared->setContextProperty("prefs", &prefs);
```

BBMusic sets them on the document and its rows reach the page through `ListItem.view` instead. If a list's rows read an app object directly, use the
root context. ([samples/ui/main.cpp](../../../samples/ui/main.cpp))

**1.3 Parent the root: `setScene()` deletes a replaced scene.** `setScene()` deletes a scene it replaces when the Application owns it, and the
Application owns one that came without a parent. BBMusic's root therefore gets `root->setParent(document)`, and `app.setScene(0)` runs before
`main` returns. A scene that has a parent must be taken off the Application before it is deleted. [app: BBMusic `main.cpp`, comment]

**1.4 `qDebug` goes nowhere.** On the phone `qDebug` and `qWarning`, QML errors included, are not shown unless routed. HyperBerry and Tengram install a
handler with `qInstallMsgHandler` that writes to the system log (`slog2`, read with `slog2info -b <app>`) and to `data/logs/ui.log`, flushing every line
(a freeze or a kill keeps what was written; the system recycles a closed app's log within minutes). Phoenix and BBMusic have a `uiLog()` that appends
timestamped lines to `ui.log` / `bbmusic-ui.log` and write the QML errors there. [Q10, 2026-11-02] Never log a secret (the engine's token). Cascades
logs "Invalid scaling method." for every `setScalingMethod(Fill)`: Tengram drops that line in its writer. [Classic] (`tengram` commit `ad5ae3d`)

**1.5 QML numbers reach C++ as doubles.** `JsonDataAccess` does not write a double as a plain integer, and a Go engine's integer fields refuse it
(an unix time from QML never took). The link class turns whole numbers into integers before it writes (`EngineLink::send`). Ids are strings across
the socket and in QML (JS numbers lose precision past 2^53). [Q10] (hyperberry's notes)

**1.6 `Q_INVOKABLE` with a default argument.** moc clones the method per arity; calling both crashed on another Qt 4 platform. Use two names. [untested on BB10]

**1.7 `QElapsedTimer` has no constructor.** Call `invalidate()` in your constructor, or stack garbage reads as "just sent" forever. [Q10] A
`QTcpSocket` member's destructor emits `disconnected()` after later-declared members are gone: disconnect it in the owner's destructor.

## 2. QML names that fail a whole document

None of these is caught by `lupdate`; the lint catches only some. A failed document is a black screen (root page) or a page that never opens.

| Wrong | Right | Source |
|---|---|---|
| `Component.onCompleted`, `Component.onDestruction` (no `Component` attached object: "Non-existent attached object") | `onCreationCompleted`; for destruction, let C++ watch the object's `destroyed()`. Legal only with `import QtQuick 1.0` | HyperBerry 0.0.1.179 [Q10] |
| a `bb.system` type (`SystemDialog`, `SystemToast`, `SystemListDialog`) without `import bb.system`, or with `onCreationCompleted` (they are not Cascades controls) | `import bb.system 1.2`; no `onCreationCompleted` on them | HyperBerry 5.3, 5.4 [Q10] |
| `Page { cover: ... }` | `cover` belongs to `Application`: `Application.setCover()` / `app.setCover()` | [SDK] |
| QtQuick's `Timer` names (`repeat`, `running`, `onTriggered`) on the registered `QTimer`, or the reverse | registered `QTimer`: `singleShot`, `interval`, `onTimeout`, `start()`; QtQuick 1.0 `Timer`: `repeat`, `running`, `onTriggered`. Mixing the names silently does nothing | HyperBerry [Q10] |
| `readonly property` | a plain property (the phone refuses the page: "Readonly not yet supported") | HyperBerry 0.3.1.371-373 |
| a function defined twice on a page's root object | one definition ("Duplicate method name") | Tengram 0.1.0.29 |
| a handler for a signal that does not exist | check the element's signals in the type cache | BBMusic, Phoenix scripts |
| `var` inside a binding block (`clock: { var s = ...; return ... }`) | a function | HyperBerry 0.0.1.77 [Q10] |
| `onFooChanged` reading a property bound to `foo` | read `foo` itself: the handler can run before the other bindings update | HyperBerry 0.0.1.181 [Q10] |
| `ChatController { engine: engine }` (a property named like the context property it takes) | name the property differently (`link: engine`): it bound to itself | HyperBerry 3.1 [Q10] |

**`Connections`.** No app's QML uses it. Sixteen HyperBerry and Tengram files carry the comment "Not a Connections: Cascades' QML has none" and connect with
`obj.signal.connect(fn)` in `onCreationCompleted` instead; the lint knows `Connections` under `import QtQuick 1.0`. Nothing in the sources shows a
`Connections` tried on the phone: treat it as unproven and use `.connect()`. [untested]

## 3. Pages: transitions, swapping the scene

**3.1 Making a page ahead.** Tengram's `ChatsPage` keeps one `ChatPage` made ahead (`takeChatPage`), made 0.7 s after the list is the top page
again, because making it during the pop animation stuttered it. A page takes 92 to 209 ms to build; opening a chat measured 0.16 s with nothing unread
and 0.88 s at the first unread, median, from the tap [Classic, 2026-11-20, `tengram` commit `ad5ae3d`]. Sheets and pages that are created on demand
(`ComponentDefinition.createObject()`) keep a fault to that page: BBMusic loads only `main.qml`, [`SearchPage.qml`](../../../samples/design/SearchPage.qml), [`MiniPlayer.qml`](../../../samples/design/MiniPlayer.qml) and
[`ConnectionBar.qml`](../../../samples/design/ConnectionBar.qml) at start.

**3.2 A page that is not pushed: BBMusic's Now Playing.** The player is shown by taking the whole screen: `Application.scene = page`, and giving
the old scene back on leaving (`NowPlayingPage.qml`, commits `006c696`, `45a356c`, `eee2cf5`, 2026-11-21). Reasons and rules from the code comments:
- A pushed page and a `Sheet` both slide in from the right; the user wanted the player to rise from the strip. [app]
- Create the page as a **child of the scene** (`createObject(Application.scene)`): a page without a parent becomes the Application's own when it takes
  the screen and is deleted under its own feet when it gives the screen back. [app]
- **Give the scene back from a timer or an animation's `onEnded`, not from inside an action's own handler**: doing it there, with a page pushed on a
  pane that was off the screen, closed the app (BBMusic 1.0.1.65, Queue). A page asked for from the player is pushed on the tab's pane after the scene is
  back. [app]
- Motion: in, a `ParallelAnimation` of `TranslateTransition` (`fromY: ui.du(50)`, 350 ms, `StockCurve.CubicOut`) and `FadeTransition` 0 to 1; out,
  220 ms, `CubicIn`, then swap back. The page under the strip first fades its content (`opacity = 0`) and hides its action bar
  (`actionBarVisibility: ChromeVisibility.Hidden`) so neither vanishes at a stroke; both are put back at the end. The player's own bar stays hidden
  until the rise has ended (shown from the start, it took the tabs' place at a stroke). [app, the user's wish 2026-11-21; not recorded as seen on the phone]
- A double tap must open one player (BBMusic ignores a second `open()` within 800 ms).

**3.3 Peek.** A page that needs every horizontal drag (a video's timeline, a zoomed picture) turns the pane's peek off while it is on top, and puts
back what the pane had when it goes: `onTopChanged: nav.noPeek = !!nav.top && !!nav.top.noPeek` (Phoenix [`HomeTab.qml`](../../../samples/design/HomeTab.qml), taken from Tengram). [app]

**3.4 Sheets** come from the right, as the system's do (BBMusic commit `006c696`). Page contents must fill the page: Tengram's Classic commit
`f154d37` made ScrollViews and docked ListViews fill their page (ten pages "opened in a corner"). [Classic]

## 4. Lists

- A list item is as wide as its content; a child with `spaceQuota` stretches it. List items cannot see the page's ids: put what they need on the
  `ListView` and reach it through `ListItem.view`. [Q10] More in **bb10-pitfalls**.
- **C++-drawn rows for speed (Tengram `ChatRow` and `MessageCard`, started in HyperBerry; the pattern in full: [13](../../../docs/13-design-and-screens.md) section 6).** `bb::cascades::ListItemProvider` lets C++ build a row once (`createItem`) and refill it on
  each recycle (`updateItem`) with setters instead of QML bindings; QML keeps the `ListView` and sets its `listItemProvider`. Measured on the Q10
  (0.0.1.159, 2026-11-06): a real QML message card with its menu 46 ms, without the menu 27 ms; the same in C++ 2 ms; a light text card 5 ms in QML, 0.5 ms
  in C++. [Q10] The cost: every later look change is C++. Rules from the code:
  - with a provider set, the ListView ignores its `listItemComponents`, so headers come from C++ too (`bb::cascades::Header`). [SDK]
  - a press reaches a C++ item as `ListItemListener::activate`, which the item inherits beside `CustomControl`; the long-press menu on the item works
    [Q10, 0.0.1.202]; the press colour is unchecked.
  - C++ items reach the page through the `ListView` they are made for: QML functions by `QMetaObject::invokeMethod` with `QVariant` arguments, properties
    by `property()`, changes by `connect(list, SIGNAL(fooChanged()), ...)`; that the connects find QML-declared signals is unproven on the phone. [untested]
  - try the cheap fix first (read `ListItem.view.x` once per row, not in each binding); port to C++ only if the Q10 shows lag. Tengram: "no heavy QML per
    message". The call sheet, composer and settings pages stay QML.
- Photo previews and sticker stills made off the UI thread (Tengram `PreviewMaker`): a photo-heavy chat froze 6 s until they were. Round pictures made on
  the worker cut 606 of 828 ms of one chat open. [Classic] (`f154d37`, `ad5ae3d`)

## 5. Samples

- `../../../docs/07-ui-build.md` sections 5 and 6, `../../../docs/09-headless-service.md`, `../../../docs/13-design-and-screens.md` section 6.
- [samples/ui/main.cpp](../../../samples/ui/main.cpp); [samples/design/NowPlayingPage.qml](../../../samples/design/NowPlayingPage.qml), [samples/design/AppCover.qml](../../../samples/design/AppCover.qml); [samples/lists/](../../../samples/lists/); `tengram` commits `f154d37`, `ad5ae3d`.
- Written from the notes of hyperberry, bbmusic, phoenix and tengram (app names only).

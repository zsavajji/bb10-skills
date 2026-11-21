---
name: bb10-pitfalls
description: Known BlackBerry 10 / Cascades traps with their causes and phone-verified fixes — silent quits and black pages from QML errors (Component.onCompleted, bb.system imports, readonly property, duplicate functions, wrong handler names, Connections), TextField focus and selection, the Q10 physical keyboard (low-byte key codes, Alt/Shift/Sym only in real fields, Enter leaking after submit, shortcuts eating typed letters), the Classic's trackpad focus, the action menu and context menu, lists (width, scrolling, inserting, C++ rows), text, fonts and emoji, ForeignWindowControl and Screen child windows (z-order, touches, keyboard focus, snapshots), touch keyboard handlers, scene and page swaps, lifecycle refocus, headless memory budget, Qt 4.8 TLS and QtNetwork, Qt 4 and C++ gotchas. Use before writing or changing Cascades code that handles focus, text input, keys, menus, lists, overlays, native windows or networking, when a BB10 app behaves differently on the phone than expected, and when cleaning up code that carries device workarounds.
---

# BB10 pitfalls

Entry form: symptom -> cause -> fix, then the tag and where it was seen. Evidence tags: **[Q10]** confirmed on the user's Q10 (in earlier
work on the same phone); **[Classic]** confirmed on the user's Classic (Q20, OS 10.3.3.3216); **[Passport]** from a friend's Passport
log; **[SDK]** read in the 10.3.1 headers or type info; **[app]** done by one of our apps (HyperBerry, Tengram, BBMusic, Phoenix) or a
third-party app, with the entry saying which and whether the phone showed it; **[other platform]** seen on Harmattan (Qt 4.7), not on
BB10; **[untested]** reasoned, not seen on a phone. Notes and reference apps are examples: when they disagree with the SDK or the
phone, the SDK and the phone win.

**Rule zero: never remove a phone-verified workaround in a clean-up** on the theory that it is no longer needed. Say it looks
redundant and let a phone test decide. (An earlier app dropped the address bar's first-key emulation because the selection was now
visible; typing appended again. The user: "why, the address bar was perfectly working!")

More sections, in `references/`: **`lists.md`** (read when you build or change a `ListView`, a model, rows, a chat-style list);
**`text-fonts-strings.md`** (read for user text, emoji, fonts, label margins, translations). Build, engine and install traps are not
here: see the table at the end.

## Start-up and QML

- **App quits at start, no log; a page never opens or opens black** -> a QML error. Check `hasErrors()` and return; run the checks of
  bb10-build-deploy (`lupdate` parse, handler check, `cascades-qmllint.sh`); log `QmlDocument::errors()`. A scripted edit once pasted
  600 lines of main.qml twice (duplicate ids). [Q10] Every failure below is of this kind: the whole document fails.
- **The lint passes, the phone refuses** -> the lint parses with Qt 6, the phone with Qt 4.8. A binding written as a block with a `var`
  (`clock: { var s = ...; return ... }`) failed the chat page (HyperBerry 0.0.1.77). Fix: use a function; parse with the SDK's `lupdate`, which is Qt 4.8. [Q10]
- **`readonly property`** -> `lupdate` accepts it; the phone refuses the page ("Readonly not yet supported"): HyperBerry's call screen never opened (0.3.1.371-373). Fix: a plain property. [Q10]
- **"Duplicate method name"** -> a function defined twice on a page's root object parses fine and the phone refuses the page (Tengram 0.1.0.29, `ChatPage`'s `picked`). `lupdate` does not see it; a `sed`/`uniq -d` check in the build script does. [app, seen on the phone: Tengram 0.1.0.29]
- **"Cannot assign to non-existent property"** -> an `onFoo:` handler for a signal the element does not have. The lint does not look at handler names; `check-qml-handlers.py` does (BBMusic and Phoenix run it). [app, seen on the phone]
- **`Component.onCompleted` / `Component.onDestruction`** -> Cascades QML has no `Component` attached object ("Non-existent attached object"); neither the lint nor `lupdate` catches it (HyperBerry 0.0.1.179 took the chat page down). Fix: `onCreationCompleted`; for clean-up let C++ watch `destroyed()`. Legal only with `import QtQuick 1.0`. [Q10]
- **`bb.system` types (`SystemDialog`, `SystemListDialog`, `SystemToast`)** are not Cascades controls: they need `import bb.system 1.2` and have no `onCreationCompleted`. Either mistake fails the document; for the root page, a black screen. [Q10] (HyperBerry 5.3, 5.4)
- **`Page { cover: SceneCover {...} }` does not exist** -> `cover` belongs to `Application` (`Application.setCover()` / `app.setCover()`); the app would quit at start. [SDK]
- **`Timer` is not a Cascades type**: `qmlRegisterType<QTimer>("uri", 1, 0, "Timer")` before `QmlDocument::create`, then `import uri 1.0`. [Q10] Its names are `singleShot`, `interval`, `onTimeout`; QtQuick 1.0's `Timer` uses `repeat`, `running`, `onTriggered`. Mixing them silently does nothing. [Q10] Add the registered type to `EXTRA` in `check-qml-handlers.py`.
- **`Connections`: settled by the code, not by the notes.** `tengram/docs/ui-fork.md` says Cascades' QML has none; HyperBerry's pitfalls say it is fine with `import QtQuick 1.0`. Grep of `*/app/assets` (2026-11-21): **no app's QML uses `Connections`**; sixteen HyperBerry/Tengram files say "Not a Connections: Cascades' QML has none" and connect with `obj.signal.connect(fn)` in `onCreationCompleted`. The lint knows `Connections` under `import QtQuick 1.0`, but no source shows it tried on a phone. Use `.connect()`. [untested]
- **No QtQuick visual items** (`Rectangle`, `Item`, `MouseArea`, `Text`, `Image`) -> they do not render in a Cascades scene. `import QtQuick 1.0` non-visuals (`QtObject`, `Timer`) work [app].
- **QML JavaScript is ES5.** An `if` without braces followed by two lines runs the second always (it slipped in while adding trace lines). Brace every multi-line `if`; no `let`, arrows, template strings.
- **An `onFooChanged` handler reads a stale property** -> it can run before the bindings on the same property: `onCallChanged` read `phase: call.state` and saw the previous state (HyperBerry 0.0.1.181). Fix: read the changed property itself. [Q10] **In `onActiveTabChanged` the bare `activeTab` is the signal's argument** ("Cannot assign to read-only property", BBMusic 0.1.0.44): write `root.activeTab`. [app]
- **A binding reads the object's own properties first** -> `ChatController { engine: engine }` bound the property to itself. Fix: another name than the context property (`link: engine`). [Q10]
- **A control's "changed" signal also fires when code changes it** -> BBMusic's scrubber sent a seek on every `valueChanged` and restarted the song whenever the page opened. Fix: act on a finger (`onTouch`), not on a value. `Slider` has both `value` and `immediateValue`. [Q10] (0.1.0.5)
- **Two `Button`s at their preferred widths can take a whole row** -> give the part that must stay tappable its own `spaceQuota`. [Q10] (BBMusic)
- **`visible: false` on a control left it on screen** (Phoenix 0.1.0.6: a `SegmentedControl` stayed on Home under the title bar). Fix: wrap it in a `Container` and set `visible` there. [Q10]
- **List rows read a context property as nothing** -> properties set on the document's context do not reach a list item's context. Fix: set them on `QmlDocument::defaultDeclarativeEngine()->rootContext()` (Phoenix 0.1.0.14). [app, seen on the phone]
- **Pages opening in a corner** -> a `ScrollView` or docked `ListView` that does not fill its page ([Classic], Tengram `f154d37`; see `references/lists.md`).
- **`Application.menuEnabled` defaults to true**: with no `Menu.definition`, swiping down slides in an **empty** drawer -> `app.setMenuEnabled(false)`. Keep the menu when you move it from a `TabbedPane` to a `NavigationPane`. [Q10]
- **`AbstractBpsEventHandler` subscribed to the navigator domain never got `NAVIGATOR_SWIPE_DOWN`** in a Cascades app -> `bb::UIToolkitSupport::swipedDown(bb::SwipeDownMode::Type)` works [Q10]. (`bb::Application::swipeDown()` also exists [SDK, untested].)
- **A permission missing from the bar descriptor fails silently** (the API does nothing) -> check `access_internet`, `access_shared`, `run_when_backgrounded` and the rest first.
- **`CONFIG += cascades10`** turns a release build into `lib<name>.so`; a descriptor expecting an executable fails with `Invalid asset path`. [built]
- **`Q_INVOKABLE` with a default argument** -> moc clones the method per arity; on Harmattan's QML1 calling both arities segfaulted. An earlier app's `scrollPage(dx, dy, pixels = false)` was called with 2 and 3 arguments on Classic/Passport paths the Q10 cannot test: prefer two names. [other platform]
- **64-bit ids lose precision as JS numbers above 2^53** -> pass them to QML as strings (also in requests such as `before`). **QML numbers reach C++ as doubles**; `JsonDataAccess` does not write those as plain integers and a Go engine's integer fields refuse them: turn whole numbers into integers before writing. [Q10]
- **`qDebug`/`qWarning` go nowhere on the phone** (QML errors included) -> route them with `qInstallMsgHandler` to a file and the system log, flush every line (the system recycles a closed app's log within minutes). [Q10, 2026-11-02]
- **Swapping the scene** (`Application.scene = page`, BBMusic's Now Playing): the page must be created with the scene as parent, and the scene must be given back from a timer or an animation's `onEnded`, not inside an action's own handler: doing it there, with a page pushed on a pane that was off the screen, closed the app (BBMusic 1.0.1.65, Queue). `setScene()` deletes a replaced scene the Application owns: parent the root. Motion and details: bb10-cascades `references/writing-notes.md` section 3. [app]

## TextField, focus, selection

- **A finger touching anything else takes the focus at touch-down**, before the tap completes: a list shown "while the field is focused" vanishes under the finger. Tie such UI to your own `typing` state; decide Back's meaning at touch-down (`onTouch: if (event.isDown())`). [Q10]
- **Setting `text` from code fires `onTextChanging`** (and cursor moves report the text unchanged). Remember what you set and ignore a change equal to it. [Q10]
- **Select-all on focus is undone by the tap**: Cascades then puts the cursor where the finger landed. Re-selecting from `editor.cursorPositionChanged` within ~500 ms of focus sometimes left the keyboard **unable to type at all**. Fix: a transparent `Container` with a `TapHandler` over the unfocused field (DockLayout sibling, `visible: !field.focused`) that calls `field.requestFocus()`, so there is no tap point. [Q10]
- **The Q10 keyboard collapses the selection before the first key lands**, so typing appends even with the address visibly selected. Fix: "first key replaces" emulation: diff old vs new text in `onTextChanging`, keep only what the key inserted (a first Backspace clears), unless the user moved the cursor themselves (>500 ms after focus, >150 ms before the edit). [Q10]
- **Cascades refocuses the last focused TextField when the app returns to full screen**, even after `loseFocus()`; keys meant elsewhere go into it. Fix: a flag set only by the user's tap; `onFocusedChanged: if (focused && !asked) { loseFocus(); return }`. [Q10]
- `requestFocus()` works whatever the `focusPolicy`, even `None` [SDK]. `focusRetentionPolicyFlags: FocusRetentionPolicy.LoseToFocusable` keeps focus when non-focusable controls (bar buttons) are tapped; it applies to user-initiated changes only. [SDK, Q10]
- **`clearButtonVisible: true` + a long selected text**: the end selection handle parks on the X or pokes out of a custom pill. Keep the system X and make the field ~20 px narrower (`rightPadding: 20` on its wrapper). A custom X appearing beside the field resizes it ("the field just jumps around"). [Q10]
- **`TextInputActionItem.status` showed nothing** with the composer empty and unfocused: a state the user must see goes in the page, over the action bar. [Q10] (HyperBerry 4.4b)

## Physical keyboard (Q10)

- **Keys through a `KeyListener` bypass the input method**: no tapped/locked Alt and Shift, no system ALT/Shift badge, no Sym picker, no held-key accents. Only a focused Cascades text field gets them. Fix for typing into anything else (a web page, a terminal): a **proxy `TextField`** focused while the target wants text. `onTextChanging`: forward `text`, then `text = ""`; a `KeyListener` on it forwards Backspace/Delete/Tab/Escape; Enter via `input.submitKey: SubmitKey.EnterKey` + `submitKeyFocusBehavior: SubmitKeyFocusBehavior.Keep`; `input.flags` with AutoCapitalizationOff, AutoCorrectionOff, PredictionOff, SpellCheckOff, WordSubstitutionOff and AutoPeriodOff (an emptied field otherwise capitalises every key); `focusPolicy: FocusPolicy.None` and `navigation.focusPolicy: NavigationFocusPolicy.NotFocusable` (or it becomes the page's default Enter target). [Q10: "shift and symbols work perfectly as system"]
- The proxy must be **opaque but visually hidden**: `opacity: 0` stops the Sym picker opening for it; `visible: false` cannot take focus. Use `backgroundVisible: false` and stack it under something else (an earlier DockLayout sibling). [Q10]
- **Key codes cut to their low byte**: Shift `0xF0E1` -> `0xE1` (a-acute), right Shift `0xE2`, Ctrl `0xE3`/`0xE4`, Alt `0xF0E9` -> `0xE9` (e-acute), right Alt `0xEA`, Sym `0xF0D3` -> `0xD3`, Delete `0xF0FF` -> `0xFF`. Rule: if `event.keycap >= 0xE000` and `0x80 <= key < 0x100`, or the key is one of those modifier bytes and its keycap is equal or 0, add `0xF000`. A real "a-acute" from the symbol layer has keycap `a`, so it still types. Sym-picker symbols arrive with keycap 0. [Q10]
- Alt/Shift: a tap applies to one key, **press-and-hold locks** (not double-tap). [Q10]
- **The Enter that pressed Go comes back** to the Page's `KeyListener` once the field lets go of focus. Ignore key 13 for ~1 s after `onSubmitted`. [Q10]
- **Shortcuts stay live** on actions left on the page after their menu closes and **eat typed letters** ("impossible to type"). Binding `enabled` to "menu is visible" fixed typing, but the menu then showed no letters: it reads them as it opens. Fix: `enabled: menuKeys`, set true *before* `openActionMenu()` and false when `actionMenuVisualState` returns to `Hidden`. [Q10 for both failures; the combined fix shipped in 0.5.0.55, awaiting the phone]
- `SystemShortcut` letters follow the phone's language (Italian: C search, B bottom, A top, N new). [Q10] (HyperBerry)

## Classic: trackpad and focus

- **A custom tappable control cannot be reached by the Classic's trackpad** -> a `Container` with a `TapHandler` takes no focus. **The `TapArea` fix does not work** [Classic] (the owner, 2026-11-21). The attempt (Tengram `f154d37`, copied into HyperBerry `a908ef1`): one `TapArea.qml`, a `Container` with `gestureHandlers: TapHandler` and `keyListeners: KeyListener { onKeyReleased: if (event.key == 13 || event.key == 10) area.clicked() }` (Enter as the trackpad's click), a "lit as pressed" focus look, `focusPolicy: FocusPolicy.Key` beside a text field and `KeyAndTouch` elsewhere; 37 controls moved to it. It stays fine as a touch target. No working fix is recorded: keep every action reachable by touch and by stock controls, and treat `NavigationHandler`/`TrackpadHandler` (bb10-devices) as untried candidates. Grid tiles do open through the grid's `onTriggered`. Details: `docs/13` section 5.
- Click = tap, click-and-hold = action menu; do not remap Call/Menu/Back/End. `TrackpadHandler` (1.4) is the Classic's trackpad; none of the apps uses it. [SDK]
- **Page sizing**: pages whose content did not fill them opened "in a corner" on the Classic (`references/lists.md`). **Pages that need every horizontal drag** (a video timeline, a zoomed picture) turn the pane's `peek` off while on top (Phoenix `noPeek`). [app]
- **A gallery swipe over a `ScrollView`** -> the ScrollView under the finger takes the touch. Fix: watch it in `onTouchCapture`, not from the system's Back/overflow corners, and require a quarter of the screen across (a fifth turned pictures by accident). [app, Phoenix `ImagePage.qml`, user 2026-11-21]

## Menus

- `Page.openActionMenu()` (1.4) opens the action menu from code, even with `actionBarVisibility: ChromeVisibility.Hidden`. [Q10]
- **No API opens a context menu from code**; `ContextMenuHandler` only has `closeMenu()`. Long presses inside a foreign window cannot trigger one: use the action menu instead. [SDK]
- **`ActionItem` has no `visible`**: rebuild the set with `removeAction(actionAt(0))` + `addAction(item, ActionBarPlacement.InOverflow)`; declare the items in `attachedObjects`. `removeAction` keeps the item alive; **`removeAllActions()` deletes them**. A call action that is not wanted is removed from the list, not hidden. [SDK]
- **Swapping actions in the same instant as `openActionMenu()` makes the rows slide in from the top** instead of with the menu. Open at once if the set is unchanged; otherwise swap, then open ~150 ms later from a Timer (guard against the user having cancelled meanwhile). [Q10]
- Detect every way a menu closes (tap outside, Back): `onActionMenuVisualStateChanged` with `ActionMenuVisualState.Hidden`. [Q10]
- **An action with no icon shows a cog**, and `/usr/share/icons` has no Copy, Search, Top or Bottom icon: ship your own. [Q10] (HyperBerry 3.3, 5.5)
- `SystemListDialog` rows are small for touch and OK/Cancel may still show even with `selectionIndicator: ListSelectionIndicator.Highlight`; for a quick pick prefer the action menu. [Q10]
- **Tabs that change under the user** -> with more tabs than the bar holds, the overflow tabs swap places with the visible ones when chosen ("i don't like the fact that the tabs shuffle around"). BBMusic kept the bar constant: three, then four tabs (four "to be checked on the Q20"). [Q10] (BBMusic 0.1.0.23-25)

## ForeignWindowControl and Screen windows

- **A full-screen overlay swallows every touch under it**, even where it draws nothing (a transparent Container holding a drawer, a hint, an edge strip): the controls beneath never got a tap. Give the overlay's root `overlapTouchPolicy: OverlapTouchPolicy.Allow`; its own children still take their touches. [Q10] (HyperBerry's CallDrawer, 0.0.1.263 -> 264)
- **Native windows follow a Cascades `translationX`** on an ancestor of their ForeignWindowControl (`updatedProperties: WindowProperty.Position`): a page sliding 548 px carried its video windows with it, smoothly. [Q10] (HyperBerry 0.0.1.265)
- **A tap reaches a child's TapHandler and its parent's, in no promised order.** A debounce that keeps the first lost the child's action when the parent fired first; let the child act at once and stamp the time, and have the parent wait ~60 ms and skip when a child took the tap. [Q10]
- **A camera viewfinder window sits above Cascades**: nothing Cascades draws can cover it (a hint went under it); keep overlays out of its rectangle. [Q10]
- Binding needs: `SCREEN_CHILD_WINDOW`, `SCREEN_PROPERTY_ID_STRING` = the control's `windowId` **set before** `screen_join_window_group(main group id)` (an id set after never got the window adopted: unsized, full screen, nothing in the control, HyperBerry 0.0.1.191). For a prebuilt binary, an `LD_PRELOAD` shim can force all three (`berry_embed.c`). [Q10]
- **Z-order: two arrangements, both on the phone.** The SDK says the foreign window sits at a **negative z-order** under Cascades' hole. At z -1 the page shows but **gets no touches** (Cascades forwards only keys); Cascades controls then draw over it and their taps work. **HyperBerry's and Tengram's shipped `VideoWindow` is at z -1** (video needs no touches; `+1` also tried, under the hole kept; Q10, 0.0.1.192; [12](../../docs/12-video-and-audio.md) section 5). Above Cascades it takes touches and **nothing Cascades draws can appear over it**: no SystemToast, action menu, text magnifier, overlays. Choose by what must receive touches. [Q10]
- Workarounds that shipped for the window above Cascades [Q10]:
  - Clip the child window (`SCREEN_PROPERTY_CLIP_POSITION`/`CLIP_SIZE`, set by the parent, then `screen_flush_context`) so bars drawn by Cascades under it show at the top/bottom edges. One rectangle only: never the middle. Units are window pixels even when the buffer is smaller.
  - Hide the control (`visible: false`) while a menu, sheet, dialog or text editing is up, and show a **snapshot** in an `ImageView` in its place.
  - Messages in a strip the clip leaves uncovered; SystemToast only while the window is hidden.
- **Screen gives the keyboard to the window last touched**: a tap in the child stole keys from Cascades (they arrived raw). Set `SCREEN_SENSITIVITY_NO_FOCUS` on the child (touches still work) and block its `screen_set_group_property_pv(SCREEN_PROPERTY_FOCUS)`. Let only the Cascades process own the Navigator (no `navigator_request_events` in the child). [Q10]
- Snapshot without the other process's help: `screen_create_pixmap` on the window's context, `SCREEN_FORMAT_RGBX8888`, usage `SCREEN_USAGE_NATIVE | SCREEN_USAGE_READ` (NATIVE required), buffer size = window size, `screen_read_window(win, buf, 0, 0, 0)`. Screen's RGBX is **B,G,R,X in memory**, `bb::ImageData` RGBX is R,G,B,X: swap bytes 0 and 2. Hand Cascades `bb::cascades::Image(imageData)` directly. [Q10] (`Browser::grabPage()`)
- Anything left behind the window (a "Starting..." label) shows through whenever it hides; bind placeholders to real state. Hide the control once the child exits, or its last frame covers your Restart button. [Q10]
- `windowHandle()` is valid only after `boundToWindow`. Let the child size its own window (`updatedProperties: WindowProperty.Position | WindowProperty.Visible`) when its touch mapping depends on that size. [Q10]

## Touch keyboard, on-screen keyboard

- `TouchKeyboardHandler` (Passport) events travel **from the focused node up**; with nothing focused they reach no handler. Give a root container the focus (`root.requestFocus()`) whenever no field has it, with `focusRetentionPolicyFlags: LoseToFocusable`. [Passport, Q10 unaffected]
- The Passport's keyboard reports through `TouchKeyboardHandler`, **not** `TrackpadHandler` (that is the Classic's trackpad, 1.4). [Passport]
- With `resizeBehavior: PageResizeBehavior.None` the on-screen keyboard covers the page; lift your own bar by the BPS `virtualkeyboard_get_height()` while your field types (Passport top row: 144 px, 240 px with symbols). [Passport]

## Data, threads, files

- **Change `DataModel`s and Cascades objects only on the UI thread**; workers report back with queued signals. [app]
- QML `openDatabaseSync()` stores its database under a hashed, opaque path: C++ cannot open it without probing. If both sides need the data, own the database in C++ with an explicit path. [app]
- Call `QSqlDatabase::removeDatabase()` after the last `QSqlDatabase` copy is out of scope. [app]
- `MediaKeyWatcher` misses the mute/play key on some devices; a published app adds an application `eventFilter` for raw key codes. Sensor readings need debounce plus hysteresis. [app]
- **A `QTcpSocket` member's destructor emits `disconnected()`** after members declared later are gone: disconnect it in the owner's destructor. [Q10] (HyperBerry 2.2)
- **`QElapsedTimer` has no constructor**: it holds stack garbage until `invalidate()`; the START limit read "just sent" forever and the engine never started. [Q10]
- **A forward-declared class does not convert to its base**: passing a `PollBubble *` where a `Control *` was wanted failed until its header was included. [app]
- `QT -= gui` removes `QImage` (QtGui still links because QtDeclarative needs it).
- **C++ dialect**: GCC 4.6.3 is C++98 (no `auto`, lambdas, `nullptr`, range-for; `SIGNAL`/`SLOT`); HyperBerry and Tengram compile with GCC 4.8.3 and `-std=gnu++11` (verified on the Q10 and the Classic, 2026-11-20). Qt's `tr()`/C strings are Latin-1 unless the codecs are set (`references/text-fonts-strings.md`).

## Images, motion

- `ImageView` fades new images in by default: `loadEffect: ImageViewLoadEffect.None`. [Q10]
- `ImageView.imageSource` takes `asset:///` and `file://` paths into `data/`. [Q10]
- One `ImplicitAnimationController` per property per node; disable it when C++ drives the value (BBMusic's strip fill: `implicitLayoutAnimationsEnabled: false`, it changes four times a second). [SDK]
- `autoSize.maxLineCount` works only with `multiline: true`. [SDK, Q10]
- **Native-feeling page motion**: movement, not fades; a page that swaps the scene rises with a translate+fade (350 ms in, 220 ms out) after the page under it fades and its bar slides away ([app], BBMusic; `bb10-cascades/references/writing-notes.md`).

## Lifecycle

- **`fullscreen()`/`thumbnail()`/`invisible()` can fire twice in a row**; minimising and screen-off both fire the hidden one. Make handlers idempotent. [Q10]
- **A background app cannot invoke** ("not allowed to make invoke requests"): with the screen off, the engine came back only once the UI woke. The reply is `InvokeReplyError` 2 `BadRequest`, whose "could not be parsed" misleads. Do not retry on a short timer: a 500 ms retry kept the CPU busy for 18.5 hours (55,560 refused STARTs, Q10, 2026-11-03); double the wait on each refusal (500 ms to 60 s) and retry at once on `fullscreen()`. [Q10]
- BB10 may kill a background app without a clean exit: persist as you go. [untested]
- A live Active Frame needs only `run_when_backgrounded`: an earlier app's card updated while minimized with no headless permissions [Q10]. (`_sys_run_headless`/`_sys_headless_nostop` are for headless services.) The Active Frame is `Application`'s cover; update it only while thumbnailed, debounced. [Q10]
- **A headless engine's budget**: without a memory class its heap is capped at **3 MiB**; `_sys_headless_xlarge` gives 30 MiB, and the `rlimit-patch` addon raises it to 128 MiB. Measured, replacing the 2013 blog's "~3 MB, 7-25% CPU": an idle Go engine held 6.4 MB of data for 30 min with no kill [Q10, 2026-10-29]; BBMusic's engine used 5.6% of a core playing 157 s [Q10] and 7.7% over 9 min [Classic, 2026-11-15]; hyperberry about 3% battery an hour with the boot timer [Q10]. [09](../../docs/09-headless-service.md)
- **The boot filter `bb.action.system.STARTED` also fires right after every install**: start at boot by a recurring invoke timer (`TIMER_FIRED`) instead. [Q10] ([09](../../docs/09-headless-service.md) section 4)
- **Hub items**: ids take only letters, digits and `_`; a tap that opens the app deletes the item; new apps have no sound or vibration until the user sets them; the lock screen and the instant preview draw the account's one icon; a white glyph vanished on the light Hub. [Q10] ([09](../../docs/09-headless-service.md) section 6)

## Networking

- **Qt 4.8 TLS: "it speaks TLS 1.0 only" was wrong** (corrected 2026-11-20 in HyperBerry's notes; the user remembered 1.2). Read off the Classic (SQC100-1, 10.3.3.3216): `/usr/lib/libssl.so.2` is **OpenSSL 1.0.2g** (TLS 1.2, ECDHE-GCM suites), and `libQtNetwork.so.4.8.6` uses `SSLv23_client_method`, which takes the highest version both ends have; **only an explicit `QSsl::TlsV1` pins 1.0**. [Classic] **Not run:** a handshake from a Qt app (log `sessionCipher().protocolString()` to settle it). Still true: **no TLS 1.3, a CA store from 2018, no redirects followed**. The earlier note's observation, modern HTTPS failing with "SSL handshake failed" [Q10], was seen on the phone; the cause given for it (a 1.0 ceiling) is what was corrected, and no handshake has been logged since. Where the network lives is a choice, not a limit: HyperBerry keeps it in the engine for Go's TLS 1.3 and current roots, tests on the PC and one writer of `chats.db`. BBMusic's `docs/plan/ui.md` (2026-11-14) still says "the UI makes no network request: Qt 4.8 cannot speak modern TLS"; it predates the correction. ([01](../../docs/01-target-and-rules.md) section 4)
- In-process HTTPS: a static OpenSSL built with the NDK's GCC plus a C network library (mongoose) [app]; see bb10-build-deploy. The app that does it skips certificate checks.
- `QTcpServer` on 127.0.0.1 works with no extra permission. [Q10]
- The built-in `WebView` is 2014 WebKit (ES5, old roots): many sites fail. [Q10]

## Build and pipeline traps live in the docs

Not UI behaviour; do not look for them here.

| Trap | Where |
|---|---|
| First `throw` crashes in `libz.so.2` (`-lz` on a toolchain older than v19.1.7-4; fixed since) | [04](../../docs/04-engine-cxx20.md) section 3 |
| SIGFPE after OpenSSL init (ARM CPU probing, `armcap` patch) | [06](../../docs/06-third-party-libs.md) |
| Engine dies at 3 MiB / 30 MiB, `std::bad_alloc`, killed after 6 s, exits on the navigator's arguments | [09](../../docs/09-headless-service.md) |
| Stale build: same Package-Version keeps the old app; `-buildIdFile` | [08](../../docs/08-packaging.md) section 3 |
| "Operation not permitted" running a binary; `pathtrust`; `.so` over ssh (mmap EPERM) | [10](../../docs/10-phone-access-and-install.md) section 3 |
| App quits at start from QML: the seven checks (lupdate, handlers, lint, `check-qml`) | [07](../../docs/07-ui-build.md) section 4 |
| Stale log, `qDebug` routing, how to read a crash dump | [11](../../docs/11-testing-and-debugging.md) section 3 |
| Rootless Docker (never `chown`), sleep and OrbStack, stale file sizes | [03](../../docs/03-toolchains.md) section 4 |
| `uname` says QNX 8.0.0, `_NTO_VERSION` 800: test features, not versions | [01](../../docs/01-target-and-rules.md) section 1 |

# Lists: traps with ListView, models and rows

Read when: you build or change a `ListView`, a data model, list rows (QML or C++), a chat-style list that starts at the bottom, or a list that lags.
Entry form: symptom -> cause -> fix, tag, where seen. Tags are those of `../SKILL.md` (**[Q10]**, **[Classic]**, **[Passport]**, **[SDK]**, **[app]**,
**[other platform]**, **[untested]**). Source for entries without another one: `hyperberry/docs/plan/pitfalls.md` "Lists" (HyperBerry 0.0.1.x, Q10).

## Layout and context

- **A centred chip sits on the left** -> a list item is as wide as its content; `HorizontalAlignment.Fill` alone does not stretch it. Fix: a child with
  `spaceQuota` stretches the row across the list. [Q10] (HyperBerry 3.1)
- **A row or page content opens in a corner** -> a `ScrollView` or docked `ListView` that does not fill its page. Fix: make it fill (`Fill` alignment,
  `spaceQuota`). Ten Tengram pages (Devices, Privacy, Blocked users, Websites...) opened in a corner on the Classic until they did. [Classic] (Tengram `f154d37`)
- **A binding in a row finds no id of the page** -> list items live in a context of their own and cannot see the page's ids. Fix: put what they need on the
  `ListView` and reach it through `ListItem.view` (`item.ListItem.view.fn(...)`); the lint reports these as false positives. App objects set with
  `setContextProperty` on the *document* do not reach rows either: set them on the engine's root context
  (Phoenix 0.1.0.14: every row's `prefs.textScale` was nothing). [Q10]; [app, seen on the phone]
- **A container narrower than its child shrinks the child, it does not clip it.** A progress fill made as a clipping container around a full-width
  `ImageView` made the image slide (with `ScalingMethod.None` it is centred in its shrunk view). Fix: draw the partial image. [Q10] (HyperBerry 4.3b)
- **A `GridListLayout` list shows headers as cells and none of their items** -> the default `headerMode` is `None`. Fix: `headerMode: ListHeaderMode.Standard`. [Q10] (4.2)
- **An `ActivityIndicator` is hidden and takes no space while stopped.** Fix: set `running`, not `visible`; it keeps its own `visible` and `opacity`. [SDK]
- **A spinner sits at the top of the tab instead of the centre** -> put it in the middle of the list's space (BBMusic ShelfPage, AlbumPage). [Q10] (BBMusic 0.1.0.22)

## Scrolling and inserting

- **`scrollToPosition(End)` right after the model is set stops short** -> the rows' heights are not known yet. Fix: a chat is a `BottomToTop` list that starts at its
  newest row with no scroll call; nothing in the paging may add one. [Q10] (3.1; Tengram `ui-fork.md` 3.6)
- **`itemsChanged(AddRemove, mapper)` inserting rows above the screen threw the list to the bottom** -> `itemAdded` at either end of the model never moves the
  rows on screen. Fix: insert only at an end (HyperBerry loads whole days; Tengram holds the oldest day's header back until an older day arrives). [Q10]
- **`scrollToPosition(Beginning)` just before a row goes in at the beginning leaves that row off screen** -> the stick-to-edge policy, which otherwise follows
  such a row, does not. Fix: scroll from the model's `itemAdded`. [Q10] (3.2)
- **A new row at the edge drags the view** -> the stick-to-edge policy follows it. Fix: `stickToEdgePolicy: None` while it lands, then move back (Tengram's
  slide-in of a new message: pin the list, report the card's laid-out height, jump and move back in 110 ms; in a history "window" the policy is off). [app, 2026-11-20]
- **A list resting near its top never pages** -> a list at its edge scrolls no more, so scroll-driven paging never fires; "at the top for 30 s, nothing loaded,
  no spinner". Fix: ask for a page as soon as one can be read after an answer (`pageIfNearTop`), re-ask a request that got no answer in 15 s, and show a
  loading row at the end while one is awaited. [Classic] (Tengram `ad5ae3d`)
- **A fast flick just after a page of rows arrives shows black for up to a second** while the rows are drawn; the `ListView` already keeps only the rows on
  screen. Open. [Q10] (3.1)
- **The title bar scrolls away with the list** -> `scrollBehavior: TitleBarScrollBehavior.Sticky` keeps a status line in view. It also stopped the list being
  resized every frame while the bar moved, which made rows with five or more emoji lag. [Q10] (3.1, 4.1)
- **A chat opens with a first-frame flash or a visible jump** -> place the list while it is hidden behind the loading spinner, then fade it in; keep
  jump buttons hidden while the list is placed (the down button blinked in chats with many unread). [app] (Tengram `f154d37`, `ad5ae3d`)

## Models and rows

- **`DataModel`s and Cascades objects change only on the UI thread**; workers report back through queued signals. [app]
- **A `listItemProvider` takes every type**: with one set, the ListView ignores its `listItemComponents`, so headers come from C++ too (`bb::cascades::Header`). [SDK]
- **C++ list items reach the page through the `ListView` they are made for** (`QMetaObject::invokeMethod`, `property()`, `connect(list, SIGNAL(fooChanged()), ...)`);
  that the connects find QML-declared signals is unproven on the phone. [untested]
- **A press reaches a C++ item as `ListItemListener::activate`**, which the item must inherit beside `CustomControl`; the menu and handler go on the item itself.
  The long-press menu works [Q10, 0.0.1.202]; the press colour is unchecked.
- **Why rows in C++**: a QML message card with its menu cost 46 ms, in C++ 2 ms (Q10, 2026-11-06). Rows made in C++ keep the look property by property, and every later
  look change is C++. Details: bb10-cascades `references/writing-notes.md` section 4. [Q10]
- **A row is dear to read**: about 3 ms with its JSON and pictures (123 rows took 416 ms on the Q10). Keep pages at 50, ask the next when the first visible entry is
  within 25 of the top. [Q10] (Tengram `ui-fork.md` 3.6)
- **Photo-heavy list freezes** (6 s) -> previews and sticker stills made on the UI thread. Fix: make them on a worker and re-read the rows in 100 ms batches. [Classic] (`f154d37`)
- **Rows with `StandardListItem` cannot be told where to stop their text** (an ellipsis before a grip): BBMusic estimated the text width from a screenshot
  (`queue.js`: `textUnits`, `fit`); if it misses on another screen or font size the row must become a custom item with real padding. [Q10]

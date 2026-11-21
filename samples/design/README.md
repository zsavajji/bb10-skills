# samples/design

Screen- and design-specific code. Not built in this repo; read as reference. Docs: [13-design-and-screens.md](../../docs/13-design-and-screens.md).

| File | From | Kind | Shows |
|---|---|---|---|
| CardStyle.h | hyperberry `df45afc` app/src | verbatim | Passport zoom of card sizes |
| TapArea.qml | tengram `5a65cdf` app/assets | verbatim | Kept as the attempt that does NOT work on the Classic |
| AppCover.qml | bbmusic `1d80813` app/assets | verbatim | Active Frame cover |
| NowPlayingPage.qml | bbmusic `1d80813` app/assets | verbatim | Scene swap and rise animation |
| Composer.qml | hyperberry `df45afc` app/assets | verbatim | Chat composer row built by hand (the system one has no layout control) |
| CallSheet.qml | hyperberry `df45afc` app/assets | verbatim | Call screen as one Sheet, in the look of the stock Phone app |
| colors.h | hyperberry `df45afc` app/src | verbatim | Stable colour per key (FNV-1a, not qHash) |
| sizes.h | hyperberry `df45afc` app/src | verbatim | Locale-aware file size text |
| cover-hyperberry.qml | hyperberry `df45afc` app/assets/cover.qml | verbatim | Active Frame cover: idle and unread states in 310 x 211 px |
| main-hyperberry.qml | hyperberry `df45afc` app/assets/main.qml | verbatim | Root TabbedPane with the tab menu and a NavigationPane per tab |
| Hint.qml | bbmusic `1d80813` app/assets | verbatim | Yellow lock-screen style hint box |
| MiniPlayer.qml | bbmusic `1d80813` app/assets | verbatim | Title strip with a progress fill, tap opens Now Playing |
| ConnectionBar.qml | bbmusic `1d80813` app/assets | verbatim | Bar shown when the engine has been gone for 10 s |
| HomeTab.qml | phoenix `4e06969` app/assets | verbatim | Tab content loaded lazily by the Tab delegate: a NavigationPane |
| ImagePage.qml | phoenix `4e06969` app/assets | verbatim | Full-screen picture gallery with pinch zoom and pan |
| SearchPage.qml | bbmusic `1d80813` app/assets | verbatim | Search page with a text field and results list |

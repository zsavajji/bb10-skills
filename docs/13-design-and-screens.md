# Design and screens

Read this when: you design or size a page for BB10 and must decide what to do about the Q10's and Classic's square 720x720 screens, the Passport's 1440x1440, the tall all-touch phones, OLED against LCD, the keyboard-phone inputs, icons and splash screens.

Code referenced here is in [../samples/](../samples/). The one OS and the rooted phones: [01-target-and-rules.md](01-target-and-rules.md). Building the UI and the QML lint: [07-ui-build.md](07-ui-build.md). Video windows on a page: [12-video-and-audio.md](12-video-and-audio.md) section 5. The tables here overlap the skills [skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md) and `bb10-ux/SKILL.md` on purpose; the rest is what the four apps did and measured.

Evidence labels. **Q10** / **Classic (Q20)** = seen or measured on that phone (with date or build). **SDK profile** = the 10.3.1 SDK's `blackberry-sdk-descriptor.xml` (read by the skill, not on a phone). **Guide** = BlackBerry's 10.3 UI Guidelines digest. **Spec sheet** = public specs. **Builds; not tried on a <model>** = compiled only.

## 1. The devices

| Model | Pixels | ppi | px per du | Screen in du | Shape | Panel | Input | Icon | Active Frame (px) | Source of the row |
|---|---|---|---|---|---|---|---|---|---|---|
| **Q10** | 720x720 | 330 | **9** | 80x80 | 1:1 | OLED | keyboard; no trackpad | 90 | 310x211 | SDK profile; 9 px/du measured on the phone; frame shown whole, measured 2026-10-31 |
| Q5 | 720x720 | 330 | 9 | 80x80 | 1:1 | LCD | keyboard | 90 | 310x211 | SDK profile + guide |
| P'9983 | 720x720 | 330 | 9 | 80x80 | 1:1 | OLED | keyboard | 90 | 310x211 | SDK profile + guide |
| **Classic (Q20)** | 720x720 | 291 (guide: 294) | **8** | 90x90 | 1:1 | LCD | keyboard, optical trackpad, Call/Menu/Back/End keys | 90 | 310x211 | SDK profile; the Classic is the second test phone since 2026-11-15 |
| **Passport** | 1440x1440 | 452 | **12** | 120x120 | 1:1 | LCD | keyboard that is touch-sensitive; top row of the on-screen keyboard | 144 | 440x486 or 440x195 | SDK profile + guide |
| Z10, P'9982 | 768x1280 | 356 | **10** | 76.8x128 | 3:5 | LCD | touch only, on-screen keyboard | 110 | 334x396 | SDK profile + guide |
| Z30 | 720x1280 | 295 | **8** | 90x160 | 9:16 | OLED | touch only | 96 | 319x437 | SDK profile + guide |
| Leap | 720x1280 | 295 | 8 | 90x160 | 9:16 | LCD | touch only | 96 | 319x437 | SDK profile + guide |
| Z3 | 720x1280 (emulated) | 295 (emulated) | 8 | 90x160 | 9:16 | LCD | touch only | 96 | 319x437 | SDK profile; its panel is 540x960 at 221 ppi and it **emulates 720x1280**: target it as a Z30 |

| Model | SoC / CPU | GPU | RAM | Source |
|---|---|---|---|---|
| Q10, Q5, Z10 (STL100-2/3/4), Classic, Leap | Snapdragon S4 Plus (MSM8960), 2x Krait, 1.2-1.5 GHz | Adreno 225 | 2 GB | Q10 measured (hardware id `0x8600270a`, OS 10.3.3.3216). **The Classic has the Q10's SoC** (the owner, 2026-11-21; an earlier app's notes gave it the Z30's S4 Pro, which was wrong). The rest spec sheets |
| Z30 | S4 Pro (MSM8960T), 2x Krait, 1.7 GHz | Adreno 320 | 2 GB | spec sheet |
| Z10 (STL100-1) | TI OMAP 4470, 2x Cortex-A9, 1.5 GHz | PowerVR SGX544 | 2 GB | spec sheet |
| Passport | Snapdragon 801, 4x Krait 400, 2.26 GHz | Adreno 330 | 3 GB | an earlier app's notes |
| Z3 | Snapdragon 400, 2x Krait, 1.2 GHz | Adreno 305 | 1.5 GB | spec sheet |

([skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md), `bb10-ux/references/ui-guidelines.md` pages 120-124.) Panels and Active Frame sizes come from the guide; pixels, ppi, px/du and icons from the SDK profiles. All phones are 32-bit ARMv7; a process has about 2 GB of address space shared with graphics and network.

The Classic's 291 ppi (guide: 294) and its 8 px/du are the only Classic numbers the apps rely on; HyperBerry tells it from the Q10 by `ui.du(1) < 9` on a 720x720 screen ([`Composer.qml`](../samples/design/Composer.qml)).

## 2. Design units

| Call | What the sources say |
|---|---|
| `ui.du(n)` | design units; QML's size function. The four apps use it for every size: 408 calls in HyperBerry's QML, 446 in Tengram's, 300 in Phoenix's, 134 in BBMusic's (counted in the four apps' QML assets) |
| `ui.sdu(n)` | "rounded" (`bb10-cascades` skill). **No app uses it** |
| `ui.px(n)` | true pixels, "for hairlines" (`bb10-ux` skill): 2 px separators, 6 px timeline track. Counted 63, 49, 10 and 5 times |
| `UIConfig::du()` / `px()` in C++ | C++-drawn rows call `m_ui->du(1.5)` and `m_ui->px(2)` on the same `UIConfig` ([`MessageCard.cpp`](../samples/lists/MessageCard.cpp)) |
| `screenWidth`, `screenHeight` | `bb::device::DisplayInfo().pixelSize()` handed to QML as context properties (HyperBerry's `main.cpp`) |

Rules from the apps:

- **1 du = 9 px on the Q10 is confirmed** (a 4 du bar logged as 36 px). The Passport's 12 px/du "matched its logs" (`bb10-devices` skill). Everything else in the table is from the SDK.
- **Same pixels, different du.** The Q10 and Classic are both 720x720, 9 against 8 px/du, so the screen is 80x80 du on the Q10 and 90x90 du on the Classic, and a du layout is 12 % roomier on the Classic (90/80). Design in du and check both.
- **The Passport** is 120x120 du and 1440 px: more room, and bitmaps drawn for the Q10 are small. HyperBerry made everything bigger on it. A Passport user's screenshot (2026-11-14, one-letter messages spanning the screen; 2026-11-18, UI too small) led to a zoom: first a third, then, at the owner's word, **a fifth**:

```cpp
// The Passport draws everything a fifth bigger: at 12 px a du its 1440 px are 120 du, where the Q10
// has 80, so a du layout came out small there (a Passport user, 2026-11-18; the user tried a third,
// too much, and chose a fifth). QML gets the same numbers as "zoom" and "pxZoom" (main.cpp).
inline float zoom() { return screen() == 1440 ? 1.2f : 1.0f; }
inline float du(bb::cascades::UIConfig *ui, float n) { return ui->du(zoom() * n); }
```
([CardStyle.h](../samples/design/CardStyle.h)) QML multiplies every `ui.du`, every `ui.px` from 3 px and every font percentage by `zoom` (`zoom * ui.du(9 * bar.big)` in [`Composer.qml`](../samples/design/Composer.qml)). `pxZoom` (12/9 of the zoom) grows sizes measured in Q10 pixels (a fixed 381 px waveform, a 72 px disc). Unseen on a Passport; "builds; not tried on a Passport" (HyperBerry's status notes). Commits `41083fc` (a third) and `9c24d4d` (a fifth); the fifth wins.
- **A Classic composer** is a fifth bigger (`big = 1.2`, Classic 0.1.2.324: "it's fine").
- **Pixel sizes measured on the Q10 are written as du** by dividing by 9; Tengram's viewer calls the stock bar 97 px, "11 du, and a du of slack" ([VideoViewer.qml](../samples/video/VideoViewer.qml)). BBMusic's [`Hint.qml`](../samples/design/Hint.qml): "Sizes are the Q10's px as design units (9 px each), so other screens scale them."
- **The call screen's fixed stage.** HyperBerry's call sheet is laid out as a 720 px square (`px(n) = ui.px(n * screenWidth / 720)`) kept to the bottom; on a tall screen `extra = screenHeight - screenWidth` is added above it and the caller's picture fills it, "from the stock Z10's frames (2026-11-11)": measured on stock screenshots, not a Z10 ([`CallSheet.qml`](../samples/design/CallSheet.qml)).
- **Text sizes.** HyperBerry sizes text in percent of one shared style: body 105 %, small 85 % (capitals 20 px at 100 % on the Q10), times 72 %, times a setting 0.9 / 1.0 / 1.2 (`Settings::textScale`) and `zoom`. The guide's scale at the Z10: body 45 px, small 35, x small 30, xx small 24 (Slate Pro). No source says the apps read the system font-size setting; the guide says to honour it.

### Row heights the apps measured (Q10, 9 px/du)

| Element | px | du | Source |
|---|---|---|---|
| Title bar | 85 | 9.4 | stock Text Messages, HyperBerry's ui notes |
| Line under it | 5 | 0.5 | same |
| Chat list row | 85 + 2 separator | 9.4 | same |
| Date header | 42 | 4.7 | same |
| Action bar | 108 | 12 | same; Phoenix's compact bar buttons 108 px each, 12 du ([`VideoPage.qml`](../samples/video/VideoPage.qml), 0.1.0.24, 2026-11-21) |
| Stock action bar in Tengram's viewer | 97 | 11 | screenshot of the Back / Install bar ([VideoViewer.qml](../samples/video/VideoViewer.qml)); another bar variant than the one above |
| Signature action | 104 circle | 11.5 | `ui.md` |
| Action menu | 490 wide, 98 rows, 72 icons | 54 / 11 | `ui.md` |
| Sheet rows | 103 | 11.4 | `ui.md` |
| Mini-player strip (BBMusic) | 7.2 du | 7.2 | the user asked a fifth taller (BBMusic's ui notes) |
| Time line on a 8 du band | 8 du, track 6 px | 8 | `VideoViewer.qml` |

How many rows fit on a screen is not counted in any source; the heights are. The guide gives no title bar, action bar or list item heights ("take them from the SDK or stock measurements").

## 3. Aspect ratios and what changes

| | Square keyboard phones (Q10, Q5, Classic, P'9983, Passport) | Tall all-touch phones (Z10, Z30, Leap, Z3) |
|---|---|---|
| Orientation | portrait only; **no landscape** on 1:1 screens, the platform does not support it | portrait default; landscape only where it is better, and never forced |
| Keyboard | physical; the app keeps the whole square. Only the Passport's top row (144 px, 240 px with the symbol panel) can cover the bottom while typing | on-screen keyboard covers the bottom while a field types; `Page.resizeBehavior` says whether the page shrinks. With `PageResizeBehavior.None` lift your bar by BPS `virtualkeyboard_get_height()` |
| Active Frame | short: the system shows only the top half (Q10: 310x211; **put the important part at the top**) | tall: 319x437, 334x396 |
| Chrome | non-sticky titles and filters that scroll away, hide chrome in immersive views, semi-transparent overlays on pictures (guide p.48) | common actions in the bottom two thirds (guide p.16-17) |

(`bb10-devices` and `bb10-ux` skills, guide pages 16-17, 48, 51-55.)

How the four apps keep one layout:

- All four descriptors set `<autoOrients>false</autoOrients>`, `<aspectRatio>portrait</aspectRatio>`, `<systemChrome>none</systemChrome>` ([samples/descriptor/](../samples/descriptor/)).
- **No selectors anywhere.** None of the apps has an `assets/9ppd/`, `720x720/` or `mindw*du/` folder (checked in the four apps' QML assets); none calls `OrientationSupport`, `resizeBehavior`, `TouchKeyboardHandler`, `TrackpadHandler` or `NavigationHandler`. Layouts are du layouts that fill their parent.
- The skill's selector table, for when you need it: `assets/720x720/` exact resolution; `9ppd/`, `12ppd/` density; `minw<N>du/`, `minh<N>du/`, `minw<N>h<M>du/` window; `mindw<N>du/`, `mindh<N>du/`, `mindw<N>h<M>du/` display (`mindw76h128du` = Z10, `mindw90h160du` = Z30, `mindw120h120du` = Passport). Prefer du layouts with no selectors; use `mindw120h120du/` for Passport-only layouts and ppd folders for bitmaps. Formats were confirmed in `libbbcascades.so`; none is exercised by these apps.
- Where tall screens are special-cased, it is in code: the call sheet above, and HyperBerry's `CallVideoView` "stage is the Q10's 720 px, kept to the bottom". The READMEs say HyperBerry is "sized for square and tall screens: Q10, Q5, Classic, Passport, Z10, Z30" and "made for the Q10 ... works on the Classic, Passport and the rest"; BBMusic "made for the Q10 and the Classic, works on the Passport and the rest"; BBMusic's descriptor comment lists "Q10/Q5/Q20, Z30, Z10, Passport" for its splashes. Only the square 720x720 phones were tried.
- A page's content must fill the page: Tengram's Classic commit `f154d37` made "ScrollViews and docked ListViews fill their page (Devices, Privacy, Blocked users, Websites and 10 more opened in a corner)". An item in a list is as wide as its content; a child with `spaceQuota` stretches it (HyperBerry's pitfalls notes, Lists).
- A card is as wide as what it holds, up to **80 % of the screen** (`Card::width`), also a picture's width: on the Passport a one-letter message had spanned the screen (2026-11-14).

## 4. Panels and theme

**OLED phones (Q10, Z30, P'9983) default to the dark theme**: pure black, no bright static bars (power, burn-in) (guide p.148-149). The system draws the theme from the descriptor:

| App | `CASCADES_THEME` (`app/bar-descriptor.xml`) | Runtime switch |
|---|---|---|
| HyperBerry | `dark?primaryColor=0x12543A&amp;primaryBase=0x1BD862` | `Settings::applyTheme` toggles Dark / Bright; Bright uses primary `#1eaa61`, base `#18884e` |
| Tengram | `dark?primaryColor=0x24A1DE&amp;primaryBase=0x1B79A7` | same switch, one colour pair for both |
| BBMusic | `dark?primaryColor=0xFF0000&amp;primaryBase=0xCC0000` ("Dark for the Q10's OLED; YouTube's red as the primary colour") | none found |
| Phoenix | `dark?primaryColor=0xFF6A1A&amp;primaryBase=0xD9480F`; splash on `#121212` | none found |

- **One primary colour; the system derives the base.** Default Sky Blue `#0092CC` (base `#087099`). Other guideline pairs: Hyper Red `#FF3333` / `#CC3333`, `#DCD427` / `#B7B327`, `#779933` / `#5C7829`.
- **A runtime switch must repeat the descriptor's pair**: `CASCADES_THEME` applies before any code runs and `Settings::applyTheme` repeats it, "or a theme switch changes the accent" (Tengram's ui-fork notes 3.2). Call `applyTheme` in `main()` before the scene is built if a saved preference exists.
- HyperBerry's accent: HyperBerry's UI notes still say primary `#1bd862`, base `#14A249`; the code (2026-11-17) has primary `#12543a`, base the old bright green, "in case the trackpad's highlight follows the base (it was too dark with a dark one)". The Active Frame keeps `#1BD862` "which reads on black where Dark's #12543a accent would not". The code wins.
- 10.3 palette: dark background `#000000`, text `#F0F0F0` / `#969696` / `#7D7D7D`, neutral `#323232` / `#282828`; bright `#FFFFFF`, `#323232` / `#646464` / `#828282`, `#F0F0F0` / `#E6E6E6`. Read colours from `ui.palette` so the UI follows the theme. Other guide colours: `#00A8DF` selection and information, `#00B800` positive, `#E6B400` attention, `#D60000` error. Pick colours that work in both themes; keep white or black text on buttons and title bars.
- **LCD phones get a grey page by default, not black.** "Black only saves power on an OLED, the Q10's": the Classic and Passport default to Dark with the page `#181818` ("Settings > Appearance > Dark background", `Settings::darkGrey`, which reads `DisplayInfo().displayTechnology()` and treats a 1440 px width as LCD "in case it reports no technology"; 2026-11-19). Tengram has the same option (`nav.pageColor`, black or `#181818`; the photo and video viewers and the QR scanner stay black).
- **Bright needs its own colours.** On Bright the quiet text is `#5e5e5e` and the text `#111111`, darker than the theme's, which "read faint there" (2026-11-17); sender-name colours have a deep twin per hue ([`colors.h`](../samples/design/colors.h), `kSendersBright`) because the light ones were unreadable. A press colour is "a step off `ui.palette.plain`": `#464646` on dark, `#DCDCDC` on bright.
- No source compares contrast or reading comfort of OLED against LCD beyond the above.

## 5. Input capabilities and what they ask of the design

| Capability | Phones | What the apps do | Evidence |
|---|---|---|---|
| Physical keyboard, type to act | Q10, Q5, Classic, Passport | typing anywhere on a chat page goes to the composer; letter `Shortcut`s on actions (HyperBerry's menu: T top, B bottom, S search, C new chat; Tengram's message menu R A V C E X D F); letters off while a field types; every shortcut has a touch path | Q10, Classic |
| Alt, Shift, Sym | Q10 | a tap applies to one key, press-and-hold locks; some codes arrive cut to their low byte. Pitfalls: `bb10-pitfalls` skill "TextField, focus, selection", "Physical keyboard (Q10)" | Q10 |
| Classic trackpad | Classic | the guideline: click = tap, click-and-hold = action menu, a visible focus highlight, a logical focus path; do not remap Call/Menu/Back/End. Stock controls follow it; the apps' custom controls do not (below) | Classic: `TapArea` does not work (the owner, 2026-11-21) |
| Passport touch keyboard | Passport | `TouchKeyboardHandler`, events travel up from the focused node; none of the apps uses it | builds; not tried on a Passport |
| On-screen keyboard | Z10, Z30, Z3, Leap | none of the apps handles it (no `resizeBehavior`, no `virtualkeyboard_*`) | builds; not tried on a Z10, Z30, Z3 or Leap |

**The Classic's trackpad: the `TapArea` attempt, which does not work.** Tengram's commit `f154d37` (2026-11-20) and its copy into HyperBerry (`a908ef1`) set out to make every custom tappable control take the trackpad's focus, show a "lit as pressed" state and treat Enter (the click) as the tap. **It does not work on the Classic** (the owner, 2026-11-21: the trackpad does not work on `TapArea` controls). No working fix is recorded yet. The attempt, so nobody repeats it as it is:

```qml
Container {
    id: area
    signal clicked()
    gestureHandlers: TapHandler {
        onTapped: area.clicked()
    }
    keyListeners: KeyListener {
        onKeyReleased: {
            if (event.key == 13 || event.key == 10) { // Enter: the trackpad's click
                area.clicked()
            }
        }
    }
}
```
([TapArea.qml](../samples/design/TapArea.qml)) Use `focusPolicy: FocusPolicy.Key` beside a text field so a touch keeps the keyboard up, `KeyAndTouch` elsewhere. 37 controls moved to `TapArea`; Tengram's HANDOFF notes called the trackpad's click on them "untried on a phone from the same evening", and the owner's test since says it does not work. `TapArea` remains fine as the touch target (its `TapHandler`). For the trackpad, the SDK's own pieces are the untried candidates: `NavigationHandler` and `TrackpadHandler` (`bb.cascades 1.4`) and the `highlighter` and `belt` samples ([../skills/bb10-devices/SKILL.md](../skills/bb10-devices/SKILL.md)). Album tiles open through the grid's `onTriggered`. Tengram's "Open in player" button and the video play button take the focus under a pale veil.

Other traps (read [skills/bb10-pitfalls/SKILL.md](../skills/bb10-pitfalls/SKILL.md) before touching input): the Passport's keyboard reports through `TouchKeyboardHandler`, not `TrackpadHandler`; with nothing focused its events reach no handler, so give a root container the focus with `focusRetentionPolicyFlags: LoseToFocusable`; shortcuts stay live after their menu closes and eat letters; the Enter that submitted comes back to the page's `KeyListener`; a keyboard-phone app must not assume a touch.

## 6. Performance as a design constraint

- The phones have two Krait cores (Passport four) and Cascades draws on the UI thread: "keep the UI thread free; use as many worker threads as cores, not more" (`bb10-devices`). GPU drawing is about 2.5x faster than CPU rasterising (Skia, Q10).
- **Lists, and anything with many elements, are drawn by C++ classes, not QML.** Tengram does it for the chat list (`ChatRow`) and for every message (`MessageCard`); HyperBerry started it (`bb::cascades::ListItemProvider`, step 8.6; HyperBerry's README: "for speed on the Q10's dual-core Krait"). The measurement that started it (Q10, 0.0.1.159, 2026-11-06): a real QML card with its menu 46 ms, without the menu 27 ms; the same in C++ 2 ms; a light text card 5 ms in QML and 0.5 ms in C++. Opening a chat took 1,018-3,107 ms on 0.0.1.156 and the last visible card came at 574-936 ms from the tap on 0.0.1.164, of which the system's slide alone is about 400 ms. The pattern, read from Tengram's `src/` ([samples/lists/](../samples/lists/): `ChatRows.h`, `MessageCards.h`, [`MessageCard.h`](../samples/lists/MessageCard.h), [`AlbumBubble.h`](../samples/lists/AlbumBubble.h)):
  - **A `ListItemProvider` per list** (`ChatRows`, `MessageCards`). `createItem(list, type)` builds a row once per type ("header" and "item" for chats; "header", "service", "loading", "text", "msg", "picture" for messages). `updateItem` refills a recycled row from the model's row with one call, `show(data.toMap())`, with setters in place of QML bindings. The page only keeps the `ListView` and sets `listItemProvider: MessageCards {}` ([`ChatPage.qml`](../samples/lists/ChatPage.qml), [`ChatList.qml`](../samples/lists/ChatList.qml)).
  - **One `CustomControl` per row kind** (`ChatRow`, `MessageCard`, `ServiceCard`, `LoadingCard`), which also inherits `ListItemListener` for the press tint (`select`, `activate`, `reset`). Its parts are plain members: `Container`, `Label`, `ImageView`, `ActionSet`.
  - **Build the parts once, then show and hide them.** `ChatRow` makes every part in its constructor, since the row is small, and `show()` only sets text and visibility. `MessageCard` makes the body text, time and ticks at once and every other part (sender, quote, bubbles, reaction chips) the first time a message needs it, then keeps it and hides it when the next message does not. Rows are recycled, so a part must never keep the previous row's content.
  - **Remember what was last set** (`m_timePlace`, `m_colMax`, `m_scale`, `m_photoPath`) and skip the setter when it is the same.
  - **Big or optional parts are classes of their own**, each a `CustomControl` with a `show()`: `MediaBubble`, `AlbumBubble`, `PollBubble`, `VoiceBubble`. `AlbumBubble` has four fixed tiles, each taking its picture from the current message.
  - **The model is a C++ `DataModel`** (`ChatListModel`, `MessageModel`) whose rows are `QVariantMap`s; the card reads them with small helpers (`str("id")`, `flag("out")`).
  - **Rows reach the page through the `ListView` they are made for:** QML functions and properties by name (`m_list->property("slideId")`), and a change that needs every row refilled is a signal on the list (`textScaleChanged`, `roundChanged`) wired to the provider's `itemUpdatesNeeded` ([`MessageCards.cpp`](../samples/lists/MessageCards.cpp), [`ChatRows.cpp`](../samples/lists/ChatRows.cpp)).
  - **The cost:** every later look change is C++, and a row is as wide as its content (spacers with `spaceQuota` stretch it). Pages stay QML: the call sheet, the composer and the settings.
- **List cost findings** (HyperBerry's pitfalls notes): `TitleBarScrollBehavior.Sticky` also stopped the list being resized every frame, which made rows with five or more emoji lag; a `GridListLayout` list needs `headerMode: ListHeaderMode.Standard`; a list item cannot see the page's ids, so put what it needs on the `ListView` and reach it through `ListItem.view`; `listItemProvider` takes every type, headers included; a fast flick just after a page of rows arrives shows black for up to a second.
- **Images and glyphs.** Cascades draws text with Monotype iType. A colour emoji font drawn at 109 ppem (10.7 MB) took 15 s and 177 MB on a dense page; derived at 32 ppem (1.1 MB) about 1 s. The emoji picker uses images, not text, "images never enter the glyph cache". Fix `ImageView`'s fade: `loadEffect: ImageViewLoadEffect.None`. Tengram's sticker frames cost 4-17 ms at 180 px ("on the S4", [`tests/tgs_check.c`](../samples/third-party/tgs_check.c), in the body of tengram commit `f154d37`) and a photo-heavy chat froze 6 s until previews were made on a worker (`f154d37`).
- Bitmaps drawn for the Q10 are scaled up on the Passport; "draw them at 2x if they look soft" ([`Composer.qml`](../samples/design/Composer.qml)). **What a bigger screen costs is not measured in any source**: nothing was run on a Passport.
- Unset Label margins give about 20 px between labels; a Label's box starts about 18 px above its capitals at 170 %; margins in a `DockLayout` do not count from the container's edge, use padding (`pitfalls.md`, `phase7.md`).
- Keep the UI idle in the background: no CPU while not full screen (Tengram's ui-fork notes 3.8).

## 7. Icons, splash screens and bitmaps

| What | Sizes and rules | Who does what |
|---|---|---|
| App icon | 90 (720x720 phones), 96 (720x1280), 110 (768x1280), 144 (1440x1440). The home screen takes the closest; the skill also lists 86 | HyperBerry and Tengram ship **86, 90, 96, 110 and 144** (`<icon>` with five images). BBMusic and Phoenix ship **one 90 px `icon.png`**, "the Q10's size" |
| Splash screens | one per resolution an app can meet in portrait: 720x720, 720x1280, 768x1280, 1440x1440 (`<splashScreens>`). The guide gives no sizes. Add 1280x720 and 1280x768 only if the app rotates | all four ship the four portrait sizes |
| In-app icons | guide: 8x8 du, focal 5.5 du: **72 px Q10, 64 Classic/Z30, 80 Z10, 96 Passport**; medium 7x7 du, small 6x6 du; single colour, flat, PNG-24 with alpha; disabled 50 % opacity | BBMusic and Phoenix draw action and tab icons at **81 px (9 du on a Q10)**; HyperBerry's marks are 27 px tall (3 du); Cascades scales them. The skill says new icons should follow the du sizes; the apps did not |
| Per-density bitmaps | `assets/9ppd/`, `12ppd/`; 81 px at 9 ppd, scale x12/9 for 12 ppd | none used |
| Hub icons | the Hub's account icon and items are 81 px, "as the Hub draws it" | HyperBerry `hub_read.png` (grey), `hub_unread.png` (yellow), `hub_account.png` (white, also the lock screen and instant preview, which share one icon); Tengram `hub_*2.png`. The Hub tints nothing: the app swaps files. A white item glyph vanished on the light Hub, so items are grey and yellow |
| Active Frame | HyperBerry `cover.qml` 310x211, no touch; BBMusic `AppCover.qml` is "one layout for every card" for 310x211, 334x396 and 319x437 | the cover's scene cannot share the app's palette; colours are written out |

How the generators draw them:

- HyperBerry's [`gen-icons.py`](../samples/ui/scripts/gen-icons-hyperberry.py) (Pillow; not in the samples): "Draws the chat list's small marks ... white on transparent, so QML tints them with `ImageView.filterColor`. Each is drawn at 8x and scaled down to its size on the Q10: 3 du, 27 px tall." It also draws the app icon (`for px in (86, 90, 96, 110, 144)`) and the splash: the tile at 30 % of the width, centred on black, for each of the four resolutions. [gen-icon.py](../samples/ui/scripts/gen-app-icon.py) does the same for Tengram's paper-plane tangram; run for Tengram, HyperBerry's script would overwrite the icon with the green tile, so take its marks only (`ui-fork.md` 3.2).
- BBMusic's and Phoenix's `gen-icons.py` (Phoenix's is [samples/ui/scripts/gen-icons.py](../samples/ui/scripts/gen-icons.py)): no library but zlib, shapes are functions `(x, y) -> bool` over the unit square, 4x4 supersampling, "action icons, white on transparent, 81 x 81 px (9 du on a Q10)". BBMusic's `splash()`: the tile's red over the screen, the glyph in the middle, a third of the short side. [`gen-app-icon.py`](../samples/ui/scripts/gen-app-icon-phoenix.py) (Pillow and ImageMagick): the flame in white on an orange `#FF6A1A` rounded square; a splash is the flame in orange on `#121212`, a third of the short side high.
- The splash exists because "the app stayed black from the tap to its first frame" (HyperBerry 0.0.1.166, 2026-11-06). Draw it on the theme's background.
- Every icon colour must work on dark and bright: icons are white and tinted by `filterColor`, not baked.

## 8. The stock look the apps follow

The standard: **native first, stock-identical where a stock app exists**; measure the stock app's screenshots on the phone, divide by px/du, write it as `ui.du()` (`bb10-ux` skill, "Working method"). Rules the apps keep (full list and page numbers: `bb10-ux/SKILL.md` and `references/ui-guidelines.md`):

| Area | Rule | Where an app shows it |
|---|---|---|
| Structure | tabs at the bottom (at most 4 on the bar, the rest in the tab menu from the left); drill-down with Back far left; **no exit button** | HyperBerry: TabbedPane with the tab menu (`showTabsOnActionBar: false`) so the chat list keeps New chat as its Signature action; BBMusic: `showTabsOnActionBar: true` at the user's wish (a fourth tab "to be checked on the Q20") |
| Action bar | at most three actions; Delete never on it | BBMusic's player bar: Back, Previous, Pause (Signature), Next, overflow |
| Signature action | one, round, primary colour, floats above the bar | HyperBerry "New chat" |
| Title bar | only when the content does not show its context; never tabs in it. In sheets it carries Cancel top-left and the positive action top-right, disabled until valid | HyperBerry's `TitleBar` 85 px |
| Sheets | slide in from the right; no swipe-back on content | BBMusic commit `006c696`: "sheets come from the right" |
| Menus | ⋮ action menu, touch-and-hold context menu (never the tap action), swipe-down application menu (at most 5, Settings far right, Help far left); never build look-alikes | HyperBerry: Settings in the application menu, Help left out |
| Feedback | field errors inline; `SystemToast` 3 s; deleting important content = no confirmation, an Undo toast; critical = `SystemDialog` | HyperBerry "Delete for me" with Undo, "for everyone" asks |
| Motion | movement, not fades; sheets and cards from the right, tab menu from the left | |
| Words | optimistic, single-word verbs, "Settings" not "Options"; Delete / Close / Cancel | |

## 9. What could and could not be tested

- Physically available: **a Q10** (the owner's) and **a Classic**. Both are 720x720 keyboard phones; the Classic adds a trackpad and the Call/Menu/Back/End keys and has 8 px/du.
- Everything for the **Passport** is from SDK profiles, the guide and a friend's screenshots (2026-11-14, -18). **Z10**: the call screen's tall-screen numbers come from stock frames. **Z30, Z3, Leap**: nothing.
- So write "builds; not tried on a Passport / Z10 / Z30 / Z3 / Leap" for: the zoom, the tall-screen layouts, the on-screen keyboard, the Passport touch keyboard, the 144 px and 96 px icons, the 1440x1440 and 768x1280 splashes.
- The skills advise a one-time log line for such paths ("on-screen keyboard: N px") and a comment that marks a constant as a guess. HyperBerry's [`Composer.qml`](../samples/design/Composer.qml) has a `ponytail:` note of that kind.
- The Q10 is the only phone whose Active Frame (310x211) and whose 9 px/du were measured.

## 10. Checklist for a new page

1. Descriptor: portrait only (`autoOrients false`, `aspectRatio portrait`), `CASCADES_THEME` dark with your primary and base; repeat the same pair in `applyTheme` if you ship a switch.
2. Size in `ui.du()`; `ui.px()` only for hairlines. Divide Q10 screenshot pixels by 9. Do not use `ui.sdu`.
3. Fill the page: `ScrollView` or `ListView` docked, children with `spaceQuota`; use padding, not margins, for container edges.
4. Check the page at 80x80 du (Q10) and 90x90 du (Classic). If you ship to the Passport, multiply du, font percentages and px from 3 by one zoom; mark it "builds".
5. Use the system's action bar, title bar, menus, `SystemToast`; at most three actions; Delete in the menu; Signature action for the one high-volume task.
6. Colours from `ui.palette`; test Dark and Bright if both exist; on LCD default to grey `#181818`, not black.
7. Every tappable custom control: a touch path for every key shortcut; letter shortcuts off while a field types. On the Classic, do not count on the trackpad reaching a custom control: the `TapArea` approach (section 5) does not work there, so keep every action reachable by touch and by stock controls.
8. Lists: no heavy QML per row; read page state through `ListItem.view`; `loadEffect: ImageViewLoadEffect.None`; set label margins; `TextFormat.Plain` on user text.
9. Native windows (video, camera): section 5 of [12-video-and-audio.md](12-video-and-audio.md); keep overlays out of a viewfinder's rectangle.
10. Icons: white on transparent, tinted by `filterColor`; all five app-icon sizes plus four splash screens, or one 90 px icon as BBMusic does; Hub icons 81 px.
11. Run the QML lint (an error quits the app at start with nothing on screen), then try on the phone; say "works" only for a phone you used.

## Where sources disagree

- HyperBerry accent: HyperBerry's UI notes `#1bd862` as primary; HyperBerry's [`Settings.cpp`](../samples/cookbook/Settings-hyperberry.cpp) and descriptor `#12543A`. Code wins.
- Passport zoom: a third (`41083fc`), then a fifth (`9c24d4d`). The fifth wins.
- Stock bar height: 97 px (Tengram viewer) against 108 px (HyperBerry's UI notes, Phoenix); different bars, both measured on the Q10.

## Samples

- [samples/design/](../samples/design/): `CardStyle.h` (Passport zoom), `TapArea.qml` (the attempt that does not work on the Classic), `AppCover.qml`, `NowPlayingPage.qml`
- [samples/lists/](../samples/lists/): `ChatRows.*`, `MessageCards.*` (C++ list rows)
- [samples/descriptor/](../samples/descriptor/): the three bar-descriptor files
- [samples/ui/scripts/](../samples/ui/scripts/): `gen-icons.py`, [`gen-app-icon.py`](../samples/ui/scripts/gen-app-icon-phoenix.py)
- [samples/video/VideoViewer.qml](../samples/video/VideoViewer.qml)
- Skills: [bb10-devices](../skills/bb10-devices/SKILL.md), [bb10-ux](../skills/bb10-ux/SKILL.md), [bb10-ux/references/ui-guidelines.md](../skills/bb10-ux/references/ui-guidelines.md), [bb10-pitfalls](../skills/bb10-pitfalls/SKILL.md), [bb10-cascades](../skills/bb10-cascades/SKILL.md)
- Git commits `f154d37`, `74bb400`, `a908ef1`, `41083fc`, `9c24d4d`, `006c696`

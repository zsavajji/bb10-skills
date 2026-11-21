---
name: bb10-ux
description: BlackBerry 10 platform look, feel and interaction for Cascades apps, from the official BB10 UI Guidelines (10.3) and stock-app measurements — navigation (tabs, tab menu, drill-down, Back/peek, sheets), action bar and Signature action, action/context/application menus, toasts vs dialogs, deleting, errors, progress, settings, forms, touch targets, 10.3 colour palette and themes (OLED → dark), primary colour, Slate Pro type scale, app/action icon and Active Frame sizes, keyboard-phone shortcuts, Classic trackpad and keys, Passport keyboard gestures, square-screen rules, how a page should rise or slide, bar heights measured on the Q10, dark on OLED and grey on LCD, accent colours through CASCADES_THEME, and measured stock BB10 browser, player and Text Messages dimensions. Use when designing or reviewing a BB10 screen, making an app feel native, choosing sizes/colours/fonts/icons, wording labels and messages, or matching a stock BlackBerry app from screenshots.
---

# BB10 look, feel and interaction

The user's standard: **native first, stock-identical where a stock app exists.** Use the system's
components (action menu, TitleBar, SystemToast, the phone's keyboard behaviour) before drawing
your own, and when imitating a stock app, measure its screenshots instead of guessing.

Design doc with the apps' measurements, the per-phone layout rules and a checklist for a new page:
[13](../../docs/13-design-and-screens.md). Phone facts (px per du, panels, input): **bb10-devices**. Evidence tags as in bb10-pitfalls
(**[Q10]**, **[Classic]**, **[app]**, **[untested]**).

Sources, in order of authority: what the stock apps do on the user's phones (measured) → the
official guidelines (`references/ui-guidelines.md`, digest of
BlackBerry's 10.3 UI Guidelines (PDF) with page numbers) → reference apps
(third-party apps, BlackBerry's Cascades samples), which are examples, not rules.

## Structure and navigation

- **Tabs** live at the bottom (≤4 on the bar, the rest in the **tab menu** sliding from the left;
  `TabbedPane`, `showTabsOnActionBar`, `sidebarState`). Most-used tab in the middle. Two arrangements the user chose:
  **tab menu only** (`showTabsOnActionBar: false`: HyperBerry, and Phoenix with five tabs, "more than a bar holds without shuffling them",
  2026-11-21) so a page's own actions get the bar and its Signature action survives; **tabs on the bar** (BBMusic, at the user's wish).
  **Tabs must not shuffle**: with five tabs the two that did not fit took another's place on the bar when chosen, and the user:
  "i don't like the fact that the tabs shuffle around ... either you keep the bottom bar always the same or you go back to the slideout".
  BBMusic therefore has as many tabs as fit beside ⋮ on every tab's first page (three, then four "to be checked on the Q20"); do not add
  one without seeing that it fits. A tab's first page then has its actions in the ⋮ menu: **no Signature action on a page whose bar holds the tabs**. [Q10]
- **Drill-down** with `NavigationPane`: **Back** far left in the action bar, hierarchical, with
  swipe-right and peek. **No exit or close button, ever**; save automatically on Back.
- **Action bar**: the screen's most frequent actions, **avoid more than three**, never fill it for
  the sake of it; **Delete is never on the bar** (bottom of the ⋮ action menu).
- **Signature action** (10.3, `ActionBarPlacement.Signature`): one, for a high-volume task
  (Compose, Add); round, primary colour, floats above the bar.
- **Title bar** only when the content doesn't show its context; never tabs in it. In **sheets** it
  carries the actions: positive (Save, Send) **top-right, disabled until valid**; Cancel/Close
  top-left. Sheets slide in from the right (BBMusic `006c696`); content has no swipe-back, peek only. A page that needs every horizontal drag
  (video timeline, zoomed picture) turns peek off while on top ([app], Phoenix).
- **Segmented control** for 2–4 views of the same content, "All"/most-used far left.
- **Menus** (all three are the system's; never build look-alikes):
  - ⋮ **action menu**: this screen's less frequent actions; app actions top, platform
    invocations (Share, Open in) middle, Select More and Delete bottom.
  - **context menu** (touch-and-hold): the item's common actions with icon + label; never the
    tap action ("Open").
  - **application menu** (swipe down): ≤5 app-wide actions, **Help far left, Settings far right**.
    None → `setMenuEnabled(false)`.
- **Active Frame**: one live fact (count, next item), dark, important part in the **top half**
  (keyboard phones show only that half); no controls (it takes no touch). BBMusic's: the cover fills the card, title, artist and
  "elapsed / total" on a 60% black band at its foot; a wide card (310×211) puts the art at the left, a tall one on top. HyperBerry's: the
  unread count, then the last line. Only the Q10's 310×211 was measured; the other sizes are the guide's. [app]
- **Now playing**: BB10 shows no player on the lock screen; the controls are in the volume overlay, which shows the cover and the artist
  line, **never the title** (BBMusic puts "Title – Artist" there; plain fields over Bluetooth, where the car shows its own). The overlay's
  process cannot read the app's folder: the cover is copied to shared storage ([09](../../docs/09-headless-service.md) section 6). [Q10]
- **Mini-player**: a strip with the title and the time, filled from the left as the track plays (grey paused, slight red playing): "no icon
  that reads as a button", "reduced to a strip with just the title to reserve more space", a grey up-chevron hints that a tap opens the player;
  the player's controls live in the player. 7.2 du (a fifth taller than first drawn). [Q10] (BBMusic 0.1.0.34-41)

## Feedback

| Situation | Use |
|---|---|
| Field-level error | inline, at the field, as soon as focus leaves it |
| Short confirmation or transient error | `SystemToast`: one line, 3 s; top for sheet actions, bottom for action-bar actions |
| Deleting important content (a message, a contact) | **no confirmation**; delete with an **Undo** toast |
| Deleting critical content (account, reset) | `SystemDialog`: question title, ≤2 sentences, single-word verb buttons, 1–4 horizontal |
| Wait > 3 s | progress (known length) or activity indicator, non-blocking, showing state ("5 of 7") |
| Empty list | never blank: a prompt to create or sync |
| A page that failed to load | a failed state **with Retry**, retried by itself when the engine is back; show the engine error only after it has not answered for 10 s, with a spinner meanwhile ("the app shows the engine error but then recovers"). [Q10] (BBMusic 0.1.0.30, 0.1.0.41) |
| Start-up with nothing to show yet | a native splash in the theme's colours (the app stayed black from the tap to its first frame: HyperBerry 0.0.1.166, BBMusic 0.1.0.41) |
| Success | say nothing; show it in the UI |

Settings: reached from the application menu, most important first, ≤2 levels, saved implicitly;
no setting that 80% of users would set the same way. Forms: hint text instead of labels, Enter
to the next field, Submit disabled until valid, never clear what the user typed.

## Keyboard phones (Q10, Q5, Classic, Passport)

- **Type to act**: typing on a list searches/filters; Enter submits (`input.submitKey: Go|Search|
  Send|Done...`, which also labels the touch keyboard's key).
- **Shortcuts**: single letters on actions (`Shortcut { key: "F" }`), shown in open menus; every
  shortcut also has a touch path; letter shortcuts off while a field types. Platform letters
  (`SystemShortcut`, from the SDK): Space/Shift+Space page down/up, T/B top/bottom, N/P
  next/previous, I/O zoom, R reply, F forward, E edit, S search, U undo, C compose. An earlier
  app on this phone borrowed BlackBerry OS browser habits: K bookmarks, Y history, N forward.
- Text entry behaves like the phone's own fields only in a focused Cascades `TextField`/
  `TextArea` (tapped/locked Alt and Shift, Sym picker, accents) — see bb10-pitfalls.
- **Classic**: trackpad click = default action, click-and-hold = action menu, visible focus
  highlight, logical focus path; Call/Menu/Back/End keys are the system's — don't remap.
  That is the guideline, and stock controls follow it. The apps' custom tappable controls are **not** reached by the trackpad:
  the `TapArea` attempt does not work on the Classic (the owner, 2026-11-21; bb10-pitfalls). Keep every action reachable by touch.
- **Passport**: the keyboard surface scrolls (swipe up/down), moves the cursor (double-tap then
  swipe), deletes words (swipe right-to-left); a top row of symbols/predictions covers the bottom
  of the screen while typing.
- Keyboard phones are **portrait only**; square screens: titles and filters that scroll away,
  no landscape.

## Theme, colour, type

- `CASCADES_THEME` in the bar descriptor: `dark` on OLED phones (**Q10, Z30, P'9983**: pure black,
  no bright static bars), `bright` or `dark` elsewhere; brand with one primary colour:
  `dark?primaryColor=0x0092CC&amp;primaryBase=0x087099` (it applies before any code runs; one pair per app: HyperBerry `0x12543A`/`0x1BD862`,
  Tengram `0x24A1DE`/`0x1B79A7`, BBMusic `0xFF0000`/`0xCC0000`, Phoenix `0xFF6A1A`/`0xD9480F`). Default primary is **Sky Blue #0092CC**
  (base #087099). Read colours from `ui.palette` when the UI should follow the theme. A runtime theme switch (`Settings::applyTheme`) must
  repeat the descriptor's pair or the switch changes the accent. HyperBerry's primary is the dark green and its base the old bright one,
  "in case the trackpad's highlight follows the base (it was too dark with a dark one)"; its notes still say `#1bd862` as primary: the code wins.
- **Dark on OLED, grey on LCD**: black only saves power on an OLED. On the Classic and Passport HyperBerry defaults to Dark with a
  `#181818` page (Settings, Appearance, "Dark background"; `displayTechnology()`, a 1440 px width counted as LCD); Tengram has the option too
  (viewers and the QR scanner stay black). **Bright needs its own colours**: quiet text `#5e5e5e`, text `#111111` ("read faint"), a deeper twin per
  sender hue; a press colour a step off `ui.palette.plain` (`#464646` dark, `#DCDCDC` bright). [Q10, Classic] (`docs/13` section 4)
- 10.3 palette — dark: background #000000, text #F0F0F0 / #969696 / #7D7D7D, neutral #323232 /
  #282828; bright: #FFFFFF, #323232 / #646464 / #828282, #F0F0F0 / #E6E6E6.
- **#00A8DF** is the guideline's selection/information colour (selected icons end at #01A8DF);
  the stock browser's progress line uses it.
- Slate Pro: `FontSize.XXSmall … XXLarge` (70–130 %; one step ≈ 2 px of glyph on a Q10) or
  `SystemDefaults.TextStyles.*` (BigText, TitleText, SubtitleText, BodyText, SmallText,
  PrimaryText). Guide scale at the Z10: body 45 px (9 pt), small 35, x small 30, xx small 24.
  Honour the user's font size: no source shows an app reading the system setting; HyperBerry has its own text-size setting
  (0.9 / 1.0 / 1.2) and sizes text in percent of one shared style (body 105%, small 85%, times 72%).
- Sizes in `ui.du` (px per du per device in bb10-devices); `ui.px` only for hairlines.

## Icons and sizes

| What | Size |
|---|---|
| App icon | 90 (720×720), 96 (720×1280), 110 (768×1280), 144 (1440×1440) px — ship all |
| Action bar / tab / menu / context icons | **8×8 du**: 72 px Q10, 64 Classic/Z30, 80 Z10, 96 Passport; focal area 5.5 du. **BBMusic and Phoenix draw 81×81 px (9 du on the Q10)** and Cascades scales them; HyperBerry's marks are 27 px tall (3 du). The apps shipped 81 on the Q10 (`gen-icons.py`, white on transparent, tinted by `filterColor`); the guide's 72 stays the rule for new work |
| Component icons | 7×7 du; small buttons 6×6 du |
| Custom touch targets | **≥ 101×101 px (7.2 mm) ≈ 10 du**; bigger than the visible control |
| Active Frame | Q10 310×211 px, Z30 319×437, Z10 334×396, Passport 440×486 / 440×195 |

Icons: single colour, flat, light/dark variants (#E3E3E3→#D1D1D1 on dark, #4F4F4F→#363636 on
bright), disabled = 50% opacity, PNG-24 with alpha. An action with no icon shows a cog and the system has no Copy, Search, Top or Bottom
icon: draw your own. Hub icons are 81 px; the Hub tints nothing (swap files); a white glyph vanished on the light Hub.
Apps' icons: HyperBerry and Tengram ship five sizes (86, 90, 96, 110, 144), BBMusic and Phoenix one 90 px; four splash screens
(720×720, 720×1280, 768×1280, 1440×1440).

## Words

Optimistic, plain, never blaming; helpful messages that say what to do ("Please move to an area
with a stronger wireless signal and try again.", not "Call failed."). Single-word verbs on
buttons and actions (Save, Delete, Cancel). Delete = removed from the system; Close = removed
from a view; Cancel = abandon an edit. "Settings", not "Options".

## Motion

Movement, not fades: sheets and cards slide from the right, the tab menu from the left, the
application menu pushes down, deletions animate, bars slide (an earlier app: 200 ms ease-out). The
user disliked `ImageView`'s default fade-in (`loadEffect: ImageViewLoadEffect.None`). Disable
Cascades' implicit animation where code drives a property (`ImplicitAnimationController`).

**How a page should rise** (BBMusic's player, 2026-11-21; the user: "growing to fill the screen", "an opacity animation"): a page that takes
the whole screen is neither pushed nor a sheet (both come from the right) but rises from where it was tapped, as the strip: a translate
(from 50 du down, 350 ms, ease-out) with a fade 0 to 1, and sinks 220 ms ease-in on leaving; the page under it first fades its content and
slides its bar away so neither vanishes at a stroke, and the player's own bar comes only once its content is up (shown at once it
took the tabs' place at a stroke). So a fade is right *together with* a movement the user asked for; not a fade standing in for one.
Code and the traps of swapping the scene: bb10-cascades `references/writing-notes.md` section 3. A chat opening smoothly: keep the next page made
ahead, place the list while it is hidden behind the spinner, fade it in (Tengram, 0.16 s median to the list on the Classic). [app, Classic]

## Bar heights measured on the Q10 (9 px/du) — different bars, all measured

Title bar 85 px (9.4 du) + 5 px line; action bar 108 px (12 du), and its compact form: Back and ⋮ buttons 108 px each, with the app's own
control bar between them (Phoenix `VideoPage`, `ChromeVisibility.Compact`, 0.1.0.24); the stock Back / Install bar in Tengram's viewer 97 px
(11 du); the browser's bottom bar 94 px (below); a Signature circle 104 px (11.5 du); action menu 490 px wide with 98 px rows and 72 px
icons; sheet rows 103 px. Row heights, the Q10/Classic/Passport differences and how many du fit: [13](../../docs/13-design-and-screens.md) sections 2-3.
More stock-app numbers (the stock player, Text Messages): `references/stock-apps-measured.md`.

## Stock BB10 browser, measured on the Q10 (1 du = 9 px)

Matched in an earlier app on this phone. The browser draws its own
bar (a native page window lies over Cascades), so these are stock-app numbers, not guideline ones.

| Element | Size | Colour |
|---|---|---|
| Full bottom bar incl. progress track | 10.5 du (94 px) | bar `#282828` |
| Progress track / line | track 0.5 du, line 0.3 du | track `#1a1a1a`, line `#00a8df` |
| Address pill | ≥ 8 du (72 px) tall | `#191919` |
| Address text | ~27 px glyphs; `FontSize.PercentageValue` 108 | white |
| Bar icons | ~29 px drawn; touch areas 6–8 du | white |
| Slim bar (scrolled) | 3.3 du (30 px) | `#282828`, `FontSize.XSmall` text |
| Title strip (swipe down) | 5 du; favicon 3.5 du | `#262626` |
| Root background / page placeholder | | `#262626` / `#f2f2f2` |
| Vertical separator | `ui.px(2)` × 5 du | `#3d3d3d` |

## Working method

1. Get a screenshot of the closest stock app on the target phone (the user takes them on the phone: copy the newest
   `/accounts/1000/shared/camera/IMG_*.png` into the project's `debug/<step>/`; never commit them).
2. Measure in px, divide by the phone's px/du, write it as `ui.du()`.
3. Use the system component first; check the guideline digest for placement and wording.
4. Draw custom only where the system component can't be used, and say why in a comment.
5. The user compares on the phone and gives pixel-level feedback ("2 px smaller"); apply exactly
   that. They test fast and report in one line: match each report to the logs before touching code; batch fixes into one build when
   asked; say exactly what a build contains. Check at 80×80 du (Q10) and 90×90 du (Classic).

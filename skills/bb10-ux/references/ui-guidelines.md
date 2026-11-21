# BlackBerry 10 UI Guidelines (10.3 edition) — digest

Source: BlackBerry's 10.3 UI Guidelines (PDF) (SWD-20150323101558865, 156 pages,
March 2015). `[p.N]` = PDF page. "(inferred)" = worked out, not printed. Extracted text with page
markers: `pdftotext -layout` the PDF when a detail is needed.

**Not in the guide** (take from the SDK or stock measurements): title/action bar heights, list
item heights, padding/margin standards, compact mode, title bar kinds, per-device splash sizes,
a full shortcut letter list (the SDK's `SystemShortcuts` has it), capitalization rules.

## 10.3 visual refresh [p.5–7]
- All-white or all-black canvas, by screen type (OLED → dark) or app theme. Colour only for
  focus, signature actions, alerts. Sharp squares and circles; whitespace separates content.
- Flat: no heavy components or borders. Bright flat app icons.
- Compact action bars with a prominent signature action that get out of the way.

## Principles [p.9–22]
- Content first: every piece of chrome costs content, most on keyboard phones [p.11–12].
- Titles and headers only when needed [p.12]. Pickers/drop-downs expand inline [p.11].
- Full-screen photos: tap shows an action bar that hides after a few seconds [p.11].
- Gestures contextual and reversible [p.13]; frequent item actions in the context menu [p.14–15].
- One-handed portrait use; keyboard phones are portrait-only; on all-touch phones common
  actions in the bottom two-thirds; search results may grow from the bottom [p.16–17].
- "Think platform, not application"; behave like the platform [p.21–22].

## Navigation [p.22–37]
- Models: tabs, drill-down, tabs + drill-down, or custom (only if clearly better) [p.22–26].
- **Tabs** at the bottom in the action bar, ≤4 there, the rest in the **tab menu** (sidebar
  sliding in from the left; most-used at the top; selected row in primary colour). Most-used
  tab in the middle. First hierarchy level only [p.23–24].
- **Drill-down**: Back in the action bar [p.25].
- **Back** [p.27]: hierarchical (to the parent). Far left of the action bar, icon + destination
  title (or "Back"). Three ways back: tap, swipe right on content, peek (drag Back right).
  **Never an exit/close button** (users leave by swiping up). Save automatically on Back. With
  tabs + drill-down, Back stays in the tab.
- Views: list, grid, free-form, content [p.28–29].
- **Action bar** [p.31–32]: bottom, hides in immersive views. Most frequent actions only;
  **avoid more than three**; don't fill it because there is room. **Delete never on the bar**:
  bottom of the action menu [p.32, 98].
- **Signature action** [p.31–32]: circular primary-colour background, raised above the bar,
  floats while scrolling. Only for a high-volume task (Compose).
- **Title bar** [p.33]: title, segmented control or actions. Only when context isn't apparent.
  Never tabs in it.
- **Title bar actions** [p.35, 84–85]: for sheets. Positive (Send, Save, Accept) top-right;
  Cancel/Close/Back top-left. Not for drill-down Back, except on text-input screens where the
  keyboard would hide the bar.
- **Segmented control** [p.34]: 2–4 views of the same content; most-used/"All" far left;
  remembers the last filter; directly above the content it filters; under tabs it filters
  within one tab.
- Menus overview [p.35–37]: action menu = less frequent screen actions; context menu = item
  actions without opening; application menu = app-wide (Settings, Log out, About, Help).

## Menus [p.77–82]
- **Action menu** [p.77–79]: opened by ⋮ at the right of the action bar. Only this screen's
  actions. Order: app actions top; platform invocations (Call, BBM, Share, Open in) middle;
  Select More and Delete bottom. Keyboard phones: shortcut keys while it is open.
- **Context menu** [p.79–81]: touch-and-hold on visible items. Most common actions only; never
  the default tap action ("Open"). Every item needs icon + label (icons show first, drag left
  for labels). Shortcut keys while open. Frequent-for-everyone actions go inline instead.
- **Application menu** [p.81–82, 89]: swipe down; **≤5 actions**; **Settings far right, Help far
  left**; nothing frequent, navigational, view-specific (Edit, Sort) or already on screen. It
  pushes the UI down. For items that open a view: slide the menu up, then the view in from the
  right.

## Patterns [p.82–102]
- **Forms** [p.82–83]: smart defaults, remembered values. Focus the first field only if it is a
  text field. Enter → next field (or submit for login). Accept flexible formats. Matching
  keyboard type. Hint text over labels (label only with a default value or long description),
  labels above fields. Validate as the user goes. Submit disabled until valid; tapping it
  explains what's missing (inline or toast).
- **Sheets** [p.84–85]: full-screen tasks sliding in/out from the right; no swipe-back on
  content (peek allowed). For tasks started from several places, temporary or multi-field
  tasks; not for simple ones. Positive action top-right, disabled until valid; Cancel top-left.
- **Dialogs** [p.86–87]: avoid (modal); prefer inline. < half the screen, never hiding what the
  user needs to decide. 1–4 buttons, horizontal. Short title; body ≤2 sentences (question for a
  confirmation). Single-word verb buttons. No "Don't show again" check boxes.
- **Toasts** [p.66, 88]: non-modal, one line, gone in 3 s. Position by origin: top for sheet
  actions, centre for placeless events, bottom for action-bar actions. A button only for Undo
  after delete.
- **Settings** [p.89–90]: no setting 80% of users would set the same. From the application menu
  (top right). Most important first, grouped with headers; progressive disclosure; ≤2 levels;
  title bar + action bar with Back; save implicitly.
- **Progress** [p.91–93, 107]: indicator when a wait exceeds 3 s; progress if duration known,
  activity if not; tap for pause/cancel; non-blocking (inline, toast, just above the action
  bar); dialog only when work can't run in the background (indicator in the title). Show state
  ("Uploading 5 of 7"). Fill lists as data arrives.
- **Saving** [p.94–95]: autosave on leaving. Prompt only for slow-to-recreate work. Show saves
  visually, not with a toast. Send/Save/Done inactive until valid.
- **Deleting** [p.96–98]: critical content (reset, remove an account) → confirmation dialog;
  important content (an email, contact) → **no confirmation, Undo toast**; unimportant → nothing.
  Delete at the bottom of the context/action menu. Animate removal.
- **Errors** [p.98–102]: prevent first. Least disruptive: inline at the field → toast (3 s) →
  dialog only if it must be acknowledged. Report early, suggest a fix, never clear user input
  (except passwords), don't announce success.
- **Multi-select** [p.55]: apply to the items that allow it; toast for the rest.

## Components [p.103–114]
- Picker for 2–3 linked values (date/time); drop-down for one value from a set, defaulting to the
  likely one; never a Yes/No drop-down (use a check box) [p.104].
- Lists [p.105]: text before images load; dividers separate, headers name sections.
- Buttons [p.106]: default = most likely, never destructive; single-word verbs; buttons, not
  links, for actions [p.74].
- Text field vs area [p.106–107]: hint text; prediction off for passwords, phone numbers, names.
- Check box (multiple), radio (>2 exclusive), toggle (On/Off); whole row is the touch target;
  positive labels; toggle labels name the state [p.108–109].
- Sliders for wide ranges/live preview; double-tap jumps to presets [p.109].
- **Touch targets ≥101×101 px (7.2 mm)** for custom components ≈ 10 du; target and release area
  larger than the visible control [p.110].
- Highlight on touch, not while scrolling; never on non-actionable items; disabled = dimmed
  [p.111–113].
- **Empty states**: never blank; prompt to create or sync [p.114].

## Colour and themes [p.5, 115–116, 129, 148–151]
- System palette by default; brand with **one primary colour**, the system derives the base.
  Default primary **Sky Blue #0092CC** (base Storm Blue #087099) [p.115–116].
- 10.3 palette [p.116, 151]:

| Role | Dark | Bright |
|---|---|---|
| Background | #000000 | #FFFFFF |
| Primary text | #F0F0F0 | #323232 |
| Secondary text | #969696 | #646464 |
| Tertiary text | #7D7D7D | #828282 |
| Neutral primary / base | #323232 / #282828 | #F0F0F0 / #E6E6E6 |

- Other accent pairs (primary → base): Hyper Red #FF3333 → #CC3333; #DCD427 → #B7B327;
  #779933 → #5C7829. Pick colours that work in both themes; keep white/black text on buttons
  and title bars [p.116].
- **OLED (Z30, Q10, P'9983) → dark theme**, pure black, no bright static bars (power, burn-in)
  [p.148–149]. Never colour alone [p.75].

## Typography [p.149–150]
Slate Pro (medium, regular, light, light italic). Sizes pt / px (px at the Z10's 356 ppi,
inferred): xx small 4.8/24, x small 6/30, small 7.11/35, medium 8/40, primary (body) 9/45,
large 10/50, x large 12/59, xx large 16/79. Honour the user's font size [p.75].

## Icons, Active Frames, splash [p.117–124, 128–146, 151–152]
- **App icon**: 90 (720×720), 110 (768×1280), 96 (720×1280), 144 (1440×1440) px. One metaphor,
  symbol 62% of the icon, light from the upper left [p.117–118]. Icon palette #96B800 #CC3F10
  #0098F0 #A30D7E #121212 #262626 #F8F8F8 [p.119].
- **In-app icons**: large (action bar, tabs, menus, content) **8×8 du**, focal 5.5 du →
  64/72/80/96 px at 8/9/10/12 px/du; medium 7×7 du; small 6×6 du; indicators 5×4 / 3×3 du
  [p.130, 152]. PNG-24 with alpha, pixel-snapped, light and dark versions.
  Treatment: subtle gradient, no colour when enabled; on bright #4F4F4F→#363636, on dark
  #E3E3E3→#D1D1D1, selected #3CBCE7→#01A8DF; disabled 50% opacity [p.142–143].
  Content-area colours: #00B800 positive, #E6B400 attention, #D60000 error, #00A8DF
  selection/information [p.143]. Strokes ≥4 px silhouette, ≥2 px detail, ≤3 objects [p.144].
  Words: Delete = removed from the system; Close = removed from a view; Cancel = abandon an
  edit [p.146].
- **Active Frames** [p.120–124]: only when one fact matters while minimized; else the system
  shows a snapshot. A footer with the app title is added for you.

| Screen | Frame | Notes |
|---|---|---|
| 720×1280 | 319×437 px | header 52 px |
| 768×1280 | 334×396 px | header 62 px |
| 720×720 | **310×211 px** | header 56 px, body 155 px |
| 1440×1440 | large 440×486, small 440×195 px | small: 3 cells × 146 px |

  Body 35 px, secondary 30 px, timestamps 24 px, header text 25 px, avatars 50×50 px.
  **Keyboard phones show only the top half**: important information at the top.
- **Splash** [p.124]: uncrowded, may show version/progress; no per-device sizes given.

## Keyboard phones, trackpad, Passport [p.16, 55, 57–61, 78, 81]
- Shortcuts: examples T top, C compose, S search, I/O zoom; every shortcut needs a touch
  alternative; shortcut keys in open action/context menus [p.58, 78, 81].
- Type-to-act: start typing to finish a task; never force switching keyboard ↔ touch [p.58].
- Touch keyboard [p.57]: two-finger swipe up shows it, down hides; auto-focus the field only
  when the task is mostly typing; it covers the action bar.
- **Passport keyboard** [p.55]: double-tap then swipe = cursor; swipe up/down = scroll; swipe
  down while typing = symbols; swipe right-to-left = delete word; flick up = take a suggestion.
- **Classic trackpad** [p.59–61]: visible focus highlight; click = default action, click-and-hold
  = action menu, Shift+click = select range, swipe = fast scroll; logical focus path.
  Nav keys: Call, Menu (action menu; hold = application menu), Back, End — **do not customize**.

## Gestures, orientation, motion [p.13, 27, 49, 51–55, 147]
- System bezel gestures: swipe up (minimize), swipe down (application menu), Hub, keyboard.
- **Orientation**: portrait default; landscape only where better on all-touch phones; once a
  screen rotates, the following ones must too; never force; **no landscape on 1:1 screens**.
- **Square screens** [p.48]: non-sticky titles and filters that scroll away, repeatable
  background patterns, hide chrome in immersive views, semi-transparent overlays on pictures.
- Motion: sequential, simple, ≤2 compounded transitions, user-driven [p.147]. Sheets/cards from
  the right, tab menu from the left, application menu pushes down, deletes animate.

## Notifications, sharing, cards [p.61–72]

| Mechanism | For | Not for |
|---|---|---|
| Splat on the icon | new content (podcasts, levels) | urgent or frequent things |
| Hub message | email, SMS, social messages | reminders |
| Hub notification item | travel updates, open Wi-Fi | messages |
| Dialog | calls, alarms, reminders | messages |
| Toast | one line, 3 s | anything longer |

LED blinks for Hub items and dialogs, not for a splat alone [p.67]. Share/Open in in menus,
targets alphabetical, Cancel top-left [p.67–68]. Cards: previewer slides in from the right,
composer and picker use the sheet layout [p.69–72]. Sounds rarely; .m4a 48 kHz for
notifications [p.125–128].

## Language and accessibility [p.73–75]
Optimistic, conversational; helpful, concise, positive, never blaming ("Please move to an area
with a stronger wireless signal and try again.", not "Call failed."); everyday words;
single-word verb labels. Logical order, few clickable areas, generous spacing, recoverable
actions, high contrast, system font size, alt text, accessibility properties.

## Most firmly stated don'ts
No exit button; Delete never on the action bar; ≤3 actions on the bar; Back far left and
hierarchical; sheet positive action top-right and disabled until valid; application menu ≤5
with Settings right and Help left; no "Open" in context menus; avoid dialogs, toasts 3 s with
a button only for Undo; no confirmation for ordinary deletes; no landscape on square screens;
indicator for waits > 3 s; don't remap Classic keys; every shortcut has a touch path; custom
touch targets ≥101 px; no Yes/No drop-downs or "Don't show again".

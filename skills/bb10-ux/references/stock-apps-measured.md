# Stock apps, measured on the Q10 (720 x 720, 9 px per du)

Read when: you match a stock BB10 screen (the music player, Text Messages, BBM) and want the numbers the apps already took from screenshots.
Tags: **[Q10]** measured from the user's screenshots on the Q10; **[app]** what an app built from them. Write sizes as `ui.du(px / 9)`.
Sources: HyperBerry's UI notes (2026-10-28), BBMusic's UI notes (2026-11-14), [docs/13-design-and-screens.md](../../../docs/13-design-and-screens.md) section 2.

## Stock Text Messages (HyperBerry) [Q10]

| Element | px | du | Made of |
|---|---|---|---|
| Status bar | y 0-39 | | system |
| Title bar | 85; `#282828` dark, `#f0f0f0` bright | 9.4 | system `TitleBar` |
| Line under it | 5; a gradient from the base colour at the left to the primary at the right (stock `#057dac` to `#0092cc`) | 0.5 | system, follows the theme |
| Chat list row | 85 + 2 separator | 9.4 | custom `ListItemComponent` |
| Date header | 42, small bold grey text, 3 separator | 4.7 | custom header |
| Action bar | 108 | 12 | system |
| Signature action | 104 circle in the primary colour, rising above the bar | 11.5 | system |
| Action menu | slides from the right, 490 wide, 98 rows, 72 icons, a shortcut letter at each row's top right | 54 / 11 | system |
| Attach sheet | "Cancel" top left in the primary colour, title centred, 103 rows, 71 coloured icon tiles on `#1a1a1a` | 11.4 / 8 | system `Sheet` + list |

Conversations follow BBM's layout: flat rectangular cards (not rounded bubbles) in the neutral colour on black, theirs left and yours right, each as wide as
its content up to 80% of the screen, a hairline underline (grey on theirs, primary on yours; it fills as an upload's progress), the time in small grey
type at the bottom right. Only proportions came from BBM's resized screenshots. [Q10]

## Stock music player (BBMusic) [Q10]

| Stock player | Measured | BBMusic |
|---|---|---|
| Now Playing: title, artist | y 0-90; white about 40 px, grey about 26 px | `FontSize.Large` white, `FontSize.Small` `#969696`, 0.5 du padding |
| hairline | y 91, `#323232` | a 2 px Container |
| album row with Repeat and Shuffle at its right | y 92-178 | 9.5 du; two 8 du `ImageButton`s |
| artwork | y 178-612; placeholder a black note on `#262626` | fills what is left, `AspectFill` |
| scrubber band over the artwork's lower edge | y 485-612, black at about 70% | a Container `#b0000000`, the Slider, elapsed left and "-5:54" right |
| action bar | Back, Previous, Pause (Signature), Next, ⋮ | the same as `ActionItem`s; Signature colour = the theme's |
| ⋮ menu | Play on, Previous, Pause, Next, Edit Order, Add to Favorites, Add to Playlist, View Album, Share, Set As Ringtone, Equalizer, Audio, Properties | Queue, View Album, View Artist, Start Radio, Share, Equalizer, Properties (no Stop: Pause does that) |
| library | segmented control; rows with an 88 px cover at the left edge | Search's filter is that control; rows are `StandardListItem`s with `imageSource` |

What the user decided on top of it (2026-11-14/15, 0.1.0.19-0.1.0.41), as design rules:
- **Shuffle is a toggle (a mode)**, as in the stock player, not a one-shot action.
- **In Edit Order the grip must not cover the time**: the time leads the line under the title; both lines are cut with an ellipsis before the grip.
- **Sort sorts only what is to come**; **Start Radio lets the song play on**.
- The primary colour stays the app's (YouTube's red).
- Not followed from the stock player: its tab menu and its Shuffle Signature action on the library page (the user wanted tabs on the bar and no Signature there).

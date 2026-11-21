---
name: bb10-devices
description: BlackBerry 10 device facts for app work — every model's screen, ppi, design-unit size (px per du), home-screen icon size, keyboard/trackpad/touch-keyboard input, SoC/GPU/RAM, OS version, and the asset-selector folders that pick per-device QML and images. Use when sizing UI for BB10 phones (Q10, Q5, Classic/Q20, Passport, Z10, Z30, Z3, Leap, Porsche Design), handling square vs tall screens, physical keyboards, trackpads, the Passport's touch keyboard or on-screen keyboards, choosing icon/splash sizes, planning performance on Krait CPUs, or deciding what can and cannot be tested on the user's phones (a Q10 and a Classic). The design doc with the sizes the apps measured is base-bb10/docs/13-design-and-screens.md.
---

# BlackBerry 10 devices

Evidence tags as in bb10-pitfalls: **[Q10]**, **[Classic]** (seen on the user's Classic, SQC100-1, OS 10.3.3.3216), **[Passport]**,
**[SDK]** (the SDK's device profiles), **[untested]**. The design doc that goes with this skill, with the apps' measurements and a
per-page checklist: [13](../../docs/13-design-and-screens.md). The phones and the rules about them: [01](../../docs/01-target-and-rules.md) section 6.

## Screen classes (authoritative: the 10.3.1 SDK's `blackberry-sdk-descriptor.xml`)

| Class | Phones | Pixels | ppi | **px per du** | Screen in du | Icon | Panel | Active Frame |
|---|---|---|---|---|---|---|---|---|
| Keyboard 720×720 | **Q10**, Q5, P'9983 | 720×720 | 330 | **9** | 80×80 | 90 | OLED (Q10, P'9983), LCD (Q5) | 310×211 |
| Keyboard 720×720 Classic | **Classic (Q20)** | 720×720 | 291 (guide: 294) | **8** | 90×90 | 90 | LCD | 310×211 |
| Keyboard 1440×1440 | **Passport** | 1440×1440 | 452 | **12** | 120×120 | 144 | LCD | 440×486 or 440×195 |
| All Touch 768×1280 | **Z10**, P'9982 | 768×1280 | 356 | **10** | 76.8×128 | 110 | LCD | 334×396 |
| All Touch 720×1280 | **Z30**, Leap, Z3 | 720×1280 | 295 | **8** | 90×160 | 96 | OLED (Z30), LCD (Leap, Z3) | 319×437 |

The Z3's panel is 540×960 at 221 ppi but it **emulates 720×1280 at 295 ppi**: target it as a
Z30 (guidelines p.47). Pixels, ppi, px/du and icons come from the SDK's device profiles; panels
and Active Frame sizes (px, before the system adds its title footer) from the official UI
Guidelines (see bb10-ux). **OLED phones (Q10, Z30, P'9983) should default to the dark theme.**

- **Same pixels ≠ same du**: Q10 and Classic are both 720×720 but 9 vs 8 px/du, so a `ui.du()`
  layout is 12% roomier on the Classic (the screen is 80×80 du on the Q10, 90×90 du on the Classic). Design in du, check both.
  The Classic's 291 ppi (guide: 294) and 8 px/du are [SDK] numbers; HyperBerry tells the two apart with `ui.du(1) < 9` on a
  720×720 screen and its Classic-sized composer (a fifth bigger, `big = 1.2`) was accepted on the Classic (0.1.2.324, "it's fine"). [Classic]
- Q10: 1 du = 9 px confirmed on the phone (a 4 du bar logged as 36 px). [Q10] Passport: ≈12 px/du
  matched its logs. [Passport]
- **Passport zoom**: at 12 px/du its 1440 px are 120 du where the Q10 has 80, so a du layout came out small there
  (a Passport user's screenshots, 2026-11-14 and 2026-11-18). HyperBerry draws everything a fifth bigger on 1440 px
  (`zoom() = 1.2`; a third was tried and was too much, commits `41083fc`, `9c24d4d`) and multiplies du, font percentages and
  px from 3 up. "Builds; not tried on a Passport." Bitmaps drawn for the Q10 are scaled up there: draw them at 2x if they look soft.
- **Cards** are as wide as their content, up to 80% of the screen, also a picture's width: on the Passport a one-letter message had spanned the screen.
- **Pixel sizes measured on the Q10 are written as du** by dividing by 9; sizes in Q10 pixels that must scale (a 381 px waveform) get
  their own factor (`pxZoom` = 12/9 of the zoom).
- Ship every icon size (86, 90, 96, 110, 144 px); the home screen takes the closest.
- Splash screens (optional, `<splashScreens>`): one per resolution — 720×720, 720×1280,
  768×1280, 1440×1440, plus 1280×720 and 1280×768 if the app rotates (one published app also ships
  PlayBook 1024×600/600×1024). The guidelines give no sizes; these are the screens.
- **OLED vs LCD**: black only saves power on an OLED (Q10, Z30, P'9983). On LCD phones (Classic, Passport, Q5, Z10, Leap, Z3) a
  black page is only black: HyperBerry defaults them to Dark with a `#181818` page (`Settings::darkGrey`, reading
  `DisplayInfo().displayTechnology()` and treating a 1440 px width as LCD in case it reports none; 2026-11-19); Tengram has the same option.
  The system theme itself comes from `CASCADES_THEME` in the descriptor. Design doc: [13](../../docs/13-design-and-screens.md) section 4.
- Keyboard phones are **portrait only**, and square screens have **no landscape** (the platform
  doesn't support it there). Keyboard phones get a short Active Frame (Q10: 310×211), and the
  Q10 shows all of it (measured 2026-10-31).
- Keyboard phones give the app the whole square (Q10: 720×720) and never cover it with a
  keyboard, except the Passport's top row; on touch phones the on-screen keyboard covers the
  bottom while a field types (`Page.resizeBehavior` decides whether the page shrinks).

## Input by model

| Model | Physical keyboard | Trackpad / nav keys | Touch-sensitive keyboard | On-screen keyboard |
|---|---|---|---|---|
| Q10, Q5, P'9983 | yes | no | no | no (Sym picker overlay only) |
| Classic (Q20) | yes | **optical trackpad**, belt keys (Call, Menu, Back, End) | no | no |
| Passport | yes | no | **yes** (swipes, cursor) | **top row**: numbers/symbols/predictions, 144 px, 240 px with the symbol panel |
| Z10, Z30, Z3, Leap | no | no | no | **yes** |

Cascades handlers: `TrackpadHandler` (1.4, Classic trackpad: `TrackpadEventType.Move`,
`deltaX/Y`), `NavigationHandler` (1.4, DPI-scaled focus navigation), `TouchKeyboardHandler`
(1.3, Passport keyboard surface: `touchType`, `fingerId`, `screenX/Y`; events go from the
**focused** node up, so something must hold focus), on-screen keyboard height via BPS
`virtualkeyboard_*` events. Samples: `belt` (Classic keys and trackpad), `gears` (Passport
swipes), `highlighter` (focus highlight for trackpad navigation).

**Classic trackpad** [Classic]: on stock controls, click = tap and click-and-hold = action menu. **Custom tappable controls are not
reached by it**: the `TapArea` attempt (a focusable `Container` treating Enter, key 13/10, as the click; Tengram `f154d37`) **does not
work on the Classic** (the owner, 2026-11-21), and no working fix is recorded. Keep every action reachable by touch; see bb10-pitfalls.

Q10 keyboard behaviour (verified): tapped Alt/Shift apply to one key, **press-and-hold locks**
them; the system draws the ALT/Shift badge only while an app text field has the keyboard; the Sym
key opens a picker; holding a key offers accents. Some key codes arrive cut to their low byte
(Shift `0xF0E1` → `0xE1` "á", Alt `0xF0E9` → `0xE9` "é", Sym `0xF0D3` → `0xD3`, Delete `0xF0FF` →
`0xFF`): see bb10-pitfalls. Screenshot: volume up + down.

## Hardware

| Phones | SoC | CPU | GPU | RAM |
|---|---|---|---|---|
| Passport | Snapdragon 801 | 4× Krait 400, 2.26 GHz | Adreno 330 | 3 GB |
| Z30 | Snapdragon S4 Pro (MSM8960T) | 2× Krait, 1.7 GHz | Adreno 320 | 2 GB |
| Q10, Q5, Z10 (STL100-2/3/4), Classic, Leap | Snapdragon S4 Plus (MSM8960) | 2× Krait, 1.2–1.5 GHz | Adreno 225 | 2 GB |
| Z10 (STL100-1) | TI OMAP 4470 | 2× Cortex-A9, 1.5 GHz | PowerVR SGX544 | 2 GB |
| Z3 | Snapdragon 400 | 2× Krait, 1.2 GHz | Adreno 305 | 1.5 GB |

Only the Q10 row is measured here; the Passport's comes from an earlier app's notes, the rest from
public spec sheets. **The Classic has the same SoC as the Q10** (MSM8960; the owner, 2026-11-21). An earlier app's notes listed it with the Z30's
S4 Pro, which was wrong. The user's Q10: MSM8960, OS 10.3.3.3216,
hardware id `0x8600270a`, Adreno 225 drivers in `/base/usr/lib/graphics/msm8960/`. [Q10] What the Classic did measure:
BBMusic's engine 7.7% of a core and its UI 6.0% over 9 min of playback, plugged in; data 2.4 MB (engine) and 86 MB (UI); tap to sound
0.69-1.41 s uncached, 77 ms cached (0.1.0.32, 2026-11-15); opening a Tengram chat 0.16 s with nothing unread, 0.88 s at the first
unread (median, 2026-11-20). [Classic]

- All 32-bit ARMv7, softfp userland. A process has ~2 GB of address space shared with graphics
  and network: keep big heaps (JS engines, caches) at 256–512 MB.
- Single-thread speed is the limit: a dual-core Krait stalls seconds on heavy JavaScript. Keep the
  UI thread free; use as many worker threads as cores, not more.
- GPU drawing on the Adreno is ~2.5× faster than CPU rasterising (measured with Skia on the Q10).

## OS

- **10.3.3 is the final version on every phone**; the Q10 and the Classic here both report 10.3.3.3216. No OS-version checks, no
  fallbacks for another version. Cascades API up to 10.3.1 (`bb.cascades 1.4`).
- QNX 6.6-based, but `uname` says `QNX 8.0.0` and `_NTO_VERSION` is 800: test for features, never
  for the version. libc is from 2014 (see bb10-cxx20 for what it lacks).
- BlackBerry's services (signing for BlackBerry World, BBM, push, payments, maps servers) are shut
  down; every phone is rooted and installs unsigned packages (bb10-build-deploy).

## Sizes the apps measured on the Q10 (9 px/du; full table in [13](../../docs/13-design-and-screens.md) section 2)

| Element | px | du | Source |
|---|---|---|---|
| Title bar | 85 (+5 line) | 9.4 | stock Text Messages, HyperBerry [Q10] |
| Chat list row | 85 + 2 separator | 9.4 | same |
| Action bar | 108 | 12 | same; Phoenix's compact bar: Back and overflow buttons 108 px each [Q10, 0.1.0.24] |
| Stock action bar in Tengram's viewer | 97 | 11 | screenshot of the Back / Install bar [Q10]: another bar variant |
| Signature action | 104 circle | 11.5 | HyperBerry |
| Action menu | 490 wide, 98 rows, 72 icons | 54 / 11 | HyperBerry |
| Sheet rows | 103 | 11.4 | HyperBerry |
| Mini-player strip (BBMusic) | 7.2 du | | the user asked a fifth taller |

- **Action icons**: the guideline says 8×8 du, **72 px on a Q10**; BBMusic and Phoenix draw them at **81 px (9 du)** and Cascades scales
  them (white on transparent, tinted by `filterColor`); HyperBerry's small marks are 27 px (3 du). The code is what shipped on the Q10;
  new work follows the guideline unless a phone says otherwise. In-app icon per model by the guide: 72 Q10, 64 Classic/Z30, 80 Z10, 96 Passport.
- App icons: HyperBerry and Tengram ship 86, 90, 96, 110, 144 px; BBMusic and Phoenix one 90 px `icon.png` (the Q10 takes 90). Splash
  screens in all four apps: 720×720, 720×1280, 768×1280, 1440×1440 (HyperBerry 0.0.1.166: the app stayed black from the tap to its first frame).
- Active Frame: HyperBerry `cover.qml` 310×211, no touch; BBMusic `AppCover.qml` one layout for 310×211, 334×396 and 319×437.
  **Only the Q10's** 310×211 and 9 px/du were measured; the rest are the SDK's and the guide's.
- `ui.sdu` is used by no app; `ui.px` only for hairlines (2 px separators, 6 px time-line track).

## Per-device assets (asset selectors)

Cascades resolves `asset:///x.qml` and images through selector folders under `assets/` (format
strings confirmed in `libbbcascades.so`; examples in the samples):

| Folder | Picks | Example |
|---|---|---|
| `assets/720x720/` | exact resolution | `HyperLocal-App-Template` |
| `assets/9ppd/`, `12ppd/` | pixels per du (density) | `cascadescookbookqml`, `helloforeignwindow` |
| `assets/minw<N>du/`, `minh<N>du/`, `minw<N>h<M>du/` | window at least N (×M) du | |
| `assets/mindw<N>du/`, `mindh<N>du/`, `mindw<N>h<M>du/` | display at least N (×M) du | `mindw76h128du` (Z10), `mindw90h160du` (Z30), `mindw120h120du` (Passport) |

Prefer du layouts that need no selectors; use `mindw120h120du/` for Passport-specific layouts and
ppd folders for bitmaps (e.g. 81×81 px action icons at 9 ppd; scale ×12/9 for 12ppd).

## Testing reality

The user has **a Q10 and a Classic** (the Classic is Q20, SQC100-1, OS 10.3.3.3216; both 720×720 keyboard phones). The old
There are two phones, not one Q10. **`ssh bb10rootx` is the cable, not a phone**: it reaches whichever one is plugged in.
Before naming it, run `ssh bb10rootx 'grep modelname /pps/services/deviceproperties'` (prints `modelname::Classic` on the Classic; on
2026-11-20 a Classic was reported as "the Q10" until the user said so). Never hardcode the device in code: read it (BPS `deviceinfo`,
`DisplayInfo`).

- **Can be tested**: the Classic's trackpad (tested: it does not work on `TapArea` controls), belt keys (Call, Menu, Back, End) and 8 px/du; the Q10's keyboard and 9 px/du; both 720×720.
- **Cannot be tested by the author (no phone)**: Passport (touch keyboard, top row, 1440×1440, 12 px/du), Z10, Z30, Z3, Leap (on-screen keyboard, tall
  screens). Those paths can only be checked through friends' logs and screenshots. Add a one-time log line ("touch keyboard scrolls
  the page", "on-screen keyboard: N px") and mark tunable constants as guesses in comments. Say "builds; not tried on a <model>"
  rather than "works"; name which of the two phones a claim was seen on.

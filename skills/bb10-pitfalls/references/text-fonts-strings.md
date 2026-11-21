# Text, fonts and strings

Read when: you show text a user wrote, use emoji or a custom font, set label margins or sizes, translate, or see odd characters.
Entry form: symptom -> cause -> fix, tag, where seen. Tags as in `../SKILL.md`. Source: HyperBerry's pitfalls notes "Text, fonts and strings",
its UI notes "Emoji", [docs/13-design-and-screens.md](../../../docs/13-design-and-screens.md) section 6.

- **"typing..." shows as "typingà|"; non-ASCII from C++ is garbage** -> Qt 4's `tr()` and C string literals are read as Latin-1. Fix: in `main()`,
  `QTextCodec::setCodecForTr` and `setCodecForCStrings` to UTF-8; `QTextStream` needs `setCodec("UTF-8")`; `CODECFORTR`/`CODECFORSRC = UTF-8` in the `.pro` for `lupdate`. [Q10] (Tengram, 2026-10-31)
- **Newer emoji are boxes everywhere** (also in the Hub) -> the system's emoji stop at Unicode 6, and Cascades' text engine (Monotype iType) applies no GSUB, so ZWJ
  sequences, skin tones and keycaps show as their parts; flags still draw. Fix: a `FontFaceRule` loading a `.ttf` from the assets, named beside the system's family
  (`"sans-serif, HBEmoji"`); text a style does not cover keeps the system's emoji. [Q10] (HyperBerry 0.7, 2026-10-29)
- **A custom font draws nowhere else** -> a `FontFaceRule`'s font reaches only the text styles built on it, and each rule loads the font again. Fix: one shared text style
  (HyperBerry keeps it in `main.qml`) used by every text component. [Q10] (0.7, 2.4)
- **A page of emoji takes 15 s and 177 MB** -> a colour emoji font at 109 ppem (10.7 MB). Fix: derive a font at 32 ppem (1.1 MB), about 1 s. A picker draws **images**,
  not text: "images never enter the glyph cache" (a scrolling picker churned it). `ImageView` fade: `loadEffect: ImageViewLoadEffect.None`. [Q10] (HyperBerry 0.7, 4.2)
- **A keycap (digit + U+20E3) shows a "?" box or a frame over the text before it** -> iType draws the digit and the marks after it from one font; ours has no digits, the system's
  has no frame. Fix: the shown text drops U+20E3 and the derived font maps it to an empty glyph; stored and copied text keep it. [Q10] (4.1)
- **Two stacked labels overflow an 85 px title bar; a group sender stands apart from its text** -> labels whose margins are not set get about 20 px between them
  (a "control-specific default"). Fix: set `topMargin` and `bottomMargin`. A Label's box also starts about 18 px above its capitals at 170 %. [Q10] (3.1)
- **A message that looks like markup renders as markup** -> `Label` text defaults to auto-detected format. Fix: `textFormat: TextFormat.Plain` on every text a user wrote. [SDK]
- **An "edited" label beside the time, shown through `visible`, never appeared** (cause not found). Fix: one label that holds both texts. [Q10] (3.1)
- **`autoSize.maxLineCount` does nothing** -> it works only with `multiline: true`. [SDK, Q10]
- **Every sender got the same colour** -> `qHash(QString)` keeps the string's last character in its low bits, so `qHash(key) % n` collapsed keys that share an ending
  (`...@s.whatsapp.net`; Telegram's numeric keys end in ten digits too). Fix: hash with FNV-1a. [Q10] (3.1; Tengram keeps it)
- **Weekdays or "today" in the wrong case or missing** -> `bb::utility::i18n::RelativeDateFormatter` names only two or three days back and dates the rest, and some languages
  lower-case its labels ("oggi"). Fix: write the weekdays yourself. [Q10] (2.4)
- **Quiet text unreadable on the Bright theme** -> theme greys read faint there. Fix: darker greys on Bright (`#5e5e5e` and `#111111` in HyperBerry), a deeper twin per hue for
  coloured names; a press colour a step off `ui.palette.plain`. [Q10] (2026-11-17, `docs/13` section 4)
- **A translation orphaned** -> translations are keyed by the source text; renaming a word in a QML string drops its translations unless the same replacement is made in
  every `.ts` `<source>` and `<translation>`. Fix: replace across QML, C++ and `.ts` in one pass. [app] (Tengram `ui-fork.md` 3.1)
- **Cascades logs "Invalid scaling method." for every `ImageView::setScalingMethod(Fill)`** (331 lines in 12 chat opens) though the value is valid; harmless; drop the line in your log writer. [Classic] (Tengram `ad5ae3d`)

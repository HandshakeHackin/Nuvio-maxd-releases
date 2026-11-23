# Menu languages in 1.0.0.18

Settings > App & Updates > Language offers these names in their own scripts:
English, Deutsch, Español, Français, العربية, 简体中文, 日本語 and 한국어.
Save a choice, fully close Max'd and restart. The selection is local to the app.
Audio/subtitle preferences and provider/addon content remain independent.

English and German cover the full 1,275-key catalog. Each of the six new catalogs
covers all 425 required core messages: navigation, settings labels/categories,
common values/actions, sign-in/PIN, playback controls, person labels, USB import
dialogs and this version's release notes. Remaining help, status and provider
messages use English fallback. The picker explains this. These are initial
translations; native-speaker feedback is welcome.

Arabic uses the existing FriBidi shaping and line-level text reordering, including
mixed Latin codes/numbers. Chinese (Simplified), Japanese and Korean use the
bundled Noto fonts. Screen layout, controller order and button assignments stay
in their existing positions; this does not mirror the whole interface.

## Catalogs and adding a language

- ui/locales/languages.json is the ordered registry: code, native name, text
  direction and the fixed TMDB language code.
- en.json is the canonical English key list; core.json is the minimum menu
  coverage required before a language can appear in the picker.
- Each other language JSON maps application-owned English keys to translations.
  Preserve leading/trailing spaces, printf arguments such as %zu and %02d,
  and named placeholders. Never add profile data, filenames, URLs or credentials.
- Run python3 tools/build_locales.py, then bash tests/localization.sh.
  The build uses --check, so stale generated data fails.
- catalogs.inc contains one shared UTF-8 byte string and offset tables. It
  avoids rebuilding large sets of string_view objects in every native module.
  No locale files or translation service are loaded at runtime.
- Add native renderer previews and font/search/persistence cases before offering
  a new language. Core translations are required; advanced entries can fall back.

The compiler rejects duplicate JSON keys, unknown keys, incomplete core coverage,
invalid locale metadata, empty/NUL/control text, hidden bidi controls and changed
format arguments. It preserves the exact generated table reproducibly.
Translations are authored for this app; no third-party translation API is used.

## Runtime behavior and compatibility

The picker uses the registry's names and stable language codes. Saving verifies
settings.json readback and rolls back a failed save. Cached pages switch on the
next app launch. Invalid or unsupported codes fall back to English. Common region
aliases normalize to the corresponding offered language; zh-CN/zh-SG/zh-Hans use
Simplified Chinese, while unsupported Traditional Chinese tags fall back to
English rather than being mislabeled.

Menu search folds common Latin accents/case and Arabic vowel marks/tatweel while
preserving Chinese/Japanese/Korean text. Category IDs and dialog purposes remain
stable. No process locale, numeric/protocol parsing or source/profile identity
depends on translated strings. Addon/user metadata is never passed through a
blanket renderer lookup.

People metadata requests use only the registry's eight whitelisted TMDB language
codes. Missing non-English biographies can fall back to English. URL injection
and identity/body/list limits retain their existing guards.

## Checks and remaining feedback

Catalog regression tests cover malformed/duplicate input, missing core keys,
unknown keys, stale output and printf argument/type/width changes. Host tests
exercise all eight real language-picker save/readback callbacks, failed saves,
restart behavior, invalid/region codes, category navigation/search and USB
cancellation. Every unique translated core glyph is rasterized in all three real
font roles after Arabic shaping. People request fixtures cover the expanded
language whitelist and Chinese-to-English fallback.

The renderer suite includes settings, language pickers, sign-in, PIN, home,
person, player, USB and release-note views for each new locale. These host
checks do not establish console/firmware compatibility or native-speaker review.

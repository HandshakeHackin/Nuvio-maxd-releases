# Thai subtitle test fix — issue #27

This test includes the 1.0.1.0 unified guide and previous SIMKL/subtitle fixes.
It targets two reproduced failures: Thai UTF-8 text with missing/incorrect
language metadata could fall back to a font without Thai glyphs; downloaded
Windows-874/TIS-620 text tagged as Thai was decoded through a Western code page.
The reporter has not yet supplied a version, engine, subtitle format or sample,
so confirmation against their original file is still needed.

Install the test ZIP after fully closing Max'd, using the usual folder install
method and keeping app data. This unsigned test shows a dismissible
**Unverified build** notice. Existing official releases remain unchanged.

Please test:

1. Play the same video and Thai subtitle that showed squares or question marks.
2. Try an embedded Thai track and a downloaded Thai subtitle, when available.
3. Check Native/EVO and VLC as applicable. Confirm Latin text mixed with Thai,
   Thai vowel/tone marks, size/bold choices, and seeking or switching tracks.
4. For ASS/SSA, check that authored position, colour and timing are retained.
5. Report the app version, playback engine, embedded/downloaded source and file
   type. If the problem remains, a short non-private example of the original
   subtitle bytes/text will help distinguish font coverage from encoding.

The fallback font keeps every Unicode code point from the original bundled
Inter/Noto Sans regular faces plus Noto Sans Thai. It is a separately named,
reproducible OFL font; existing fonts are unchanged. Native/EVO and VLC use the
new fallback without needing system fonts. VLC also receives the bundled ASS
font directory. Bitmap subtitles remain images and are not restyled.

UTF-8 and BOM-marked UTF-16 take precedence. Legacy Thai conversion requires
an explicit Thai language hint; other code pages are not reclassified. Literal
question marks already present in a damaged source file cannot be reconstructed.
Do not share provider URLs, credentials, account settings or private playlists.

# Search and navigation

Press **Triangle** to search. The controller keyboard now supports English,
German, Spanish, French, Hebrew, Arabic, Russian, Thai, Japanese kana and Korean
Hangul. Chinese characters and Japanese kanji use the PS5 system keyboard.
The input language is separate from the menu language and is remembered per
profile. This feature adds no accounts or new external search services.

- **D-pad / Cross:** choose letters, edit actions or a title suggestion.
- **L1 / R1:** change keyboard language. **Square:** uppercase / alternate characters.
- **Triangle:** search. **Circle:** cancel and keep the existing results.
- **Options / PS5 keyboard:** continue the draft in the console keyboard. Choose
  your configured system keyboard language there. Cancelling it keeps the draft.

Suggestions appear after two characters and a brief pause. They use only the
current profile's configured searchable addon catalogues. Choosing a suggestion
runs a normal search for its title; it does not silently start playback. Library,
Live TV, IPTV Library, Media Servers, Sports Hub and Settings searches use the
same editor with their existing search scope and empty-query behavior. Torrent
file/magnet entry keeps the system dialog so long links can still be pasted.
PINs and passwords retain their existing private input controls.

## Optional top menu

Turn on **Settings → App & Updates → Top navigation**. The side menu remains
the default. This choice applies immediately and is saved separately per profile.
The top menu includes **Search** and every enabled app section. Long menus scroll
as the focus moves, so IPTV, servers, torrents, Sports, Addons and Settings remain
reachable. Use Left from the first content item, or Up where supported, to focus
the menu. Use Left/Right and Cross to choose; Down returns to the page.

**L1 / R1** changes tabs while browsing or while the top menu is focused. Live TV
and torrent pages keep their existing shoulder shortcuts while their content is
focused. Focus the top menu to switch tabs from those pages. Playback, episode,
player-menu and person-page controls keep their existing behavior.

## Limits and privacy

Suggestions wait 500 ms after the last edit, query at most eight searchable
catalogues, use at most two requests at once, retain six short suggestions, and
limit each response to 512 KiB with a four-second request timeout and a
twelve-second query deadline. Missing/offline catalogues never prevent submitting
a search. Requests are cancelled on edits, closing the editor, opening the system
keyboard, profile changes and shutdown. Late replies cannot cross profiles or
replace a newer query. Suggestions pause while the user chooses a result.

Drafts and suggestions stay in memory and are not added to app logs or backups.
Configured catalogues necessarily receive the search text. Keyboard language and
top-menu preference are included in the existing encrypted profile backups.
System keyboard learning remains controlled by PS5 settings for ordinary search;
secret fields keep masking and learning/resume protections.

## Test on PS5

1. Try mixed Latin/Hebrew/Arabic text; switch languages without losing the draft.
   Try Korean syllables, Thai marks and kana, then use Options for Chinese/kanji.
2. Select a suggestion and confirm the correct title results. Edit quickly,
   cancel, disconnect networking and switch profiles; old suggestions must not
   reappear. Try an addon with no searchable catalogue.
3. Check local Library/TV/server/Settings searches and clearing their filters.
   Confirm profile PINs and password dialogs still use private entry.
4. Turn Top navigation on/off, reach every enabled section and the last tab,
   test Search, and verify the choice survives a restart separately per profile.
5. Check L1/R1 in the top menu, Live TV groups/favorites, torrent browsing,
   episode seasons and playback seeking. Stop/restart playback normally.

Host controller/session tests, Unicode and Hangul tests, malformed/oversized
response tests, encrypted-backup checks, translated screen renders and a complete
native app/package build are required. These do not replace testing with a PS5
controller, system IME and real user-configured addons.

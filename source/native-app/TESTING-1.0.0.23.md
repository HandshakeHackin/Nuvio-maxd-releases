# 1.0.0.23 experimental episode test

Unconfirmed fixes for the latest #22 report. Fully close Max'd before replacing the app and preserve saved data.

1. With Watch Progress = MDBList, open a partly watched series. Only episodes actually in your history should have checks. Later seasons must not become watched when another episode is marked.
2. Scroll to an episode beyond the first visible cards and mark/unmark it. Cursor, season focus and scroll position should stay in place on both the save and later refresh.
3. Revisit the series and restart. Compare its episode checks with MDBList. The invalid inferred cache from 1.0.0.22 should no longer affect checks, even before a network refresh.
4. Check genuine whole-season/show marks: their explicit episode records should still display correctly. Aggregate watched/total summary counts alone must not light any episode.
5. Check offline/pending changes, resume positions, a second profile/source, movie badges and library add/remove. No cloud history is rewritten to remove the old read-side cache.

Report firmware/install type and the outcome. Do not share credentials, account files, private URLs or raw network bodies. No live-account/physical PS5 confirmation is claimed for these fixes.

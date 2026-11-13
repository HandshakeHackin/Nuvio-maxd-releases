# 1.0.0.22 experimental watched-state test

Unconfirmed fixes for the remaining problems in #22. Close Max'd fully before replacing the app and preserve saved data.

1. With Watch Progress = MDBList, mark individual episodes watched/unwatched, leave the series, open another title, return and restart. Check both Max'd and mdblist.com.
2. Check existing whole-season/show marks. Only known episodes aired by the mark's timestamp should inherit it; future/unknown dates stay unmarked unless explicitly marked. Unmark an individual episode and revisit/restart.
3. Mark a movie watched/unwatched. Check Home, Library, Discover, Search and its details badge. Resume positions must remain unchanged.
4. If history exceeds 100 entries, check later episodes. During a failed/partial refresh, existing checks should stay visible.
5. Test a second profile/source, and verify the confirmed library add/remove fix still works.

Report version, firmware/install type and outcomes. Do not post credentials, account files, private URLs or raw response bodies. No live MDBList/physical PS5 confirmation is claimed for this build's new fixes.

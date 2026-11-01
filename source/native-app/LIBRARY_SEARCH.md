# Staged: search within your Library

Open Library and press **Triangle** or **Options**, or select the **Search** filter, to enter a title or a few words. Search covers titles already loaded from the active profile’s selected Library source: Nuvio, Trakt, SIMKL or MDBList. Guest history can be searched too.

- Search works with the existing **Show** type and **Sort** choices.
- Letter case and common Latin accents are ignored. Multiple words may appear in any order. Other scripts retain their text for matching.
- The count shows matching titles out of the loaded Library total.
- Select **Clear Search**, accept an empty keyboard entry, or press **Circle** from the Library content to restore the list. Circle again returns to the Board as before. Cancelling the keyboard preserves the current search.
- Switching profile or changing the tracker/source configuration clears the query and rejects stale keyboard replies.
- Matching stays on-device and does not request addon catalogs or send search text to trackers. Ordinary metadata/artwork loading continues for visible results. Membership and watch progress are unchanged.

## Delivery

This feature is staged after the 1.0.0.4 issue-test prerelease, alongside its two confirmed fixes. The published 1.0.0.4 downloads are immutable; assign a new version when delivering the feature.

Reporter confirmations for the included fixes:

- [Subtitle sizing confirmation (#12)](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/12#issuecomment-6560822122)
- [Trakt cover confirmation (#13)](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/13#issuecomment-6560101332)

## Validation

Production Session methods and Library filter activation are tested with offline fixtures, including title matching, type/sort combinations, progress preservation, clear/cancel, current source/profile boundaries, stale callbacks, guest history and zero search network requests. Production page-input tests cover Search/Clear access and empty results. Rendered normal, empty and long-query views are checked alongside unchanged IPTV rendering. PS5 compilation, linking, media hooks and all 414 system imports pass. Physical-console testing of this new feature remains pending.

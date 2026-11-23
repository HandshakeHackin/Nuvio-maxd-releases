# 1.0.0.24 experimental component test

This build changes compiler and library versions. Console compatibility is unconfirmed. Fully close Max’d before updating, retain the previous build and preserve saved data.

1. Compare the same H.264 and HEVC Main10 SDR/HDR10 material with Native, EVO and VLC. Check startup, pause, seek/Skip Intro, audio/subtitle switching, EOF and next episode. Confirm 10-bit pictures with padded decoder pitch look correct.
2. Test direct HTTP/HLS playback and your normal addon/provider searches. Check HTML/XML selector providers, cancellation and a failed request. Do not share playable/private URLs, credentials or raw responses.
3. Check Arabic, German and the other menu languages, subtitle text and font sizing. Confirm glyphs, Arabic shaping, wrapping and focus remain readable at your display size.
4. Recheck MDBList library add/remove, movie badges, explicit episode checks after revisit/restart and carousel position when marking a card after scrolling right. The 1.0.0.23 fixes remain in place.
5. Test an existing encrypted configuration backup and a new round trip in the matching profile. Preserve the original backup and review the restore preview. Never post account files or backup passwords.
6. Check USB M3U import, profile-local copies, media servers and your usual torrent/DHT/UPnP setup. Report generic error codes and redacted per-mount diagnostics only.

Report firmware, ZIP/FFPFSC install type, playback engine and which check failed. This build does not add the standalone casting, file-browser or web interfaces of the reference players.

# Issue #25 subtitle test fix

Issue #25 reports disappearing in-file captions and subtitle-download resolving
timeouts. No console log, sample, playback engine or firmware has been supplied;
these changes address reproducible failures and still need reporter confirmation.

- A burst of 480 small decoded packets across 24 language tracks discarded
  currently visible captions under the previous global 256-packet queue limit.
  The queue now holds up to 4096 packets under the existing 32 MiB byte budget.
  Container storage is charged to that budget as well. This provides bounded
  read-ahead headroom for Native/EVO; extreme overload can still discard packets.
  VLC draws its own embedded captions and does not use this queue.
- Only subtitle-file GETs receive a 30-second connection window and one retry
  for a DNS/connect/timeout failure before a final response or body arrives,
  including a failure connecting to a redirected CDN. Both
  attempts share a 60-second curl transfer budget. TLS failures and HTTP error
  responses are not retried. No POST or account operation gains retries.
- Subtitle bodies stop at 8 MiB of decompressed data. HTTP/HTTPS redirects,
  certificate verification and the console DNS route remain in use. User-facing
  subtitle transport failures omit detailed URLs/hostnames.
- Download cancellation follows the current playback token, including the retry.
  A platform resolver call may finish before cancellation can be observed; host
  tests cannot establish PS5 resolver behavior.

Run `bash tests/playback_quality.sh`, `bash tests/subtitle_download.sh` and
`bash tests/subtitle_sync_session.sh` from this folder. The caption regression
fails on the previous queue, then checks cue duration/gaps, next-cue display,
language switching and seek clearing. The HTTPS fixture covers normal redirected
gzip data, DNS/connect recovery, bounded failure, cancellation, decompression
limits, rejected TLS certificates and a connection handshake lasting 11 seconds.

For the console test, play the same affected source, choose its in-file caption
track and watch several minutes, then pause, seek and change caption languages.
Try an OpenSubtitles download, then leave playback during another download.
Report the app version, firmware, playback engine, subtitle type and exact error
text. Do not share account tokens, private media URLs, playlists or profile files.

This work does not publish a release, update the store or close the issue.

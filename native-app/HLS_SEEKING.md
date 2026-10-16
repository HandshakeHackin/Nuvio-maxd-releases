# HLS and seeking changes

Development branch: builds/v0.5.28. This source change does not replace the published 0.5.27 binaries.

HLS playlist and segment opens disable FFmpeg's persistent HTTP reuse. The demuxer must not treat our custom NetStream AVIO as FFmpeg's private HTTP URLContext. The setting also applies when HLS URLs do not end in .m3u8.

Seeking continues inside the existing FFmpeg session. A quick Matroska open promotes its input to seekable on the demux thread so FFmpeg can read deferred cues without reopening the player. If that promotion cannot seek, the UI performs the old reopen fallback with the selected audio stream and pause state retained. The fallback does not join its own demux thread.

The previous rendered picture stays up until a decoded picture from the new timeline replaces it. That target picture is shown even while paused or refilling; sound is held until a short 0.5-second refill is ready. Existing audio sample trimming and subtitle timeline resets are retained. Initial/resume opening and decoder recovery keep the full buffer target; subsequent network starvation uses the normal adaptive buffering policy. A seek to the beginning clamps the demux timestamp to the first stream timestamp, including HLS audio/video start offsets.

A newer seek interrupts a waiting NetStream reader or playlist/segment header wait. It does not turn a reader interruption into a permanent downloader failure. FFmpeg input error/EOF state is cleared before repositioning. Rapid requests are coalesced to the newest pending target. Torrent piece waits still use the torrent engine's existing cancellation behavior; this change does not add a new torrent-piece interrupt mechanism.

Logs report inline/reopen seeking, actual first-picture landing and milliseconds to first picture and moving playback. These are runtime measurements, not promises of a particular provider's response time. The code adapts ProsperoTV's ideas to our existing FFmpeg player; it does not import its TS-remux backend or change Native/EVO rendering.

Host verification uses real FFmpeg and the production Player/NetStream over a local HTTP fixture, with only console decoder/audio boundaries mocked. It crosses multiple HLS segments, exercises a live playlist, MP4 and first Matroska seeks, embedded subtitle metadata, selected audio, paused/rapid/backward/zero/EOF seeks, intentionally refused inline seeking, stalled reads and stalled headers. Existing player lifecycle checks cover sample trimming, decoder recovery, Auto stereo policy and bitstream-to-PCM recovery. PS5 target compilation checks the changed player/network translation units. This is not an end-to-end PS5 playback test or a newly packaged release.

References and exact commits are recorded in dependencies.json. Optional VLC integration is assessed separately in VLC_INTEGRATION.md.

The VLC integration also fixes a shared NetStream seek race: a forward skip is
only applied to an active response, so a reconnect cannot reset its byte discard
and mislabel earlier data as the new position. Discard is evaluated after a
full-buffer wait, and superseded headers cannot publish response state. The
transport regression holds a reconnect between two seeks and checks actual
returned bytes. This protects Native/EVO and VLC's HTTP access adapter.

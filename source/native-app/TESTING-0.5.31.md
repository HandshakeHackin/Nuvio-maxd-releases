# 0.5.31 tracker and playback test build

Related request: https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/2

Implemented:
- Trakt, SIMKL and MDBList account entries in Settings → Account & Services → Tracking accounts.
- Independent Library Source and Watch Progress selectors. Both default to Nuvio;
  connecting a service never opts it in automatically.
- Connected tracker sources become selectable. Library reads use Trakt/MDBList
  watchlists or SIMKL's tracked library. Custom lists are a later extension.
- Selected tracker resume/completion and watched marks, Trakt token refresh,
  SIMKL activity-aware caching, paginated lists/history and offline write retry.
- Separate connections, source choices, resume caches and queues per profile.
  Failed reads temporarily use Nuvio and retain the preferred source.
- A title rejected permanently by a tracker stays local without blocking other titles.

Read [TRACKING.md](TRACKING.md) for connection setup and limitations. This local
build does not bundle registered Trakt/SIMKL app identities. Their phone pairing
needs developer setup until the app owner supplies registrations for the release.
MDBList accepts a personal API key. Credential import is explicit and requires
compatible records in the selected Nuvio profile.

Before public release, test live pairing and reconnect, real-account library
loading, another-device resume, manual watched/unwatched marks, completion,
restart, network failure/recovery and switching profiles on a PS5. Automated
fixtures use fake accounts and cannot establish those results.

The native player remains the default. Playback additions in this local revision:
- Prefer BGR HDR10 and change the displayed buffer attributes in place. Never
  unregister/re-register the on-screen buffers after a format rejection. RGB
  remains a fallback only on an invalid-format error. Confirmation still requires
  VideoOut HDR readback after presentation; this is not a guaranteed HDR fix.
- Adapt ProsperoTube's PTS-based low-rate scheduler to the accepted 120 Hz path.
  The GPU completes before the scheduled swap; repeat pictures do not reschedule.
  Pause, buffering, timeline changes and HDR mode changes reset pacing. 60 Hz and
  high-rate video keep the existing presentation policy. VLC is not paced by this
  adapter. The 119.88 Hz calibration needs validation on the user's output.
- Retain owned decoder-frame copies; add GL fences before texture overwrite and
  resource teardown. A timed-out or failed fence stops upload rather than reusing
  a busy texture; teardown drains GPU work before releasing it.

The bitstream-to-PCM recovery check also shares the port-switch lock, avoiding
a transient closed port being misclassified as a permanent audio failure.

PS5 checks for this revision:
1. Compare the same 24/23.976 fps SDR and HDR10 clips at 60 Hz and Prefer 120 Hz
   (restart required). Check slow pans, audio sync and shown/dropped/buffer logs.
2. Try Native HDR10; save [HDR] lines including format, driver change and after
   flip status. A refused HDR format must leave output intact; SDR conversion
   remains available. TV HDR confirmation is required for acceptance.
3. Pause/resume, seek both directions, Skip Intro, open/close playback controls,
   switch resolution between streams and stop/reopen repeatedly, for Native/EVO.
4. Check no green/corrupt pictures, old pictures after seeks, blackouts during
   menus or stalled closes. Capture errors/logs if any; do not infer success from
   frame counters alone.

No physical PS5 playback or HDMI/audio acceptance has been performed for these
changes. Native HDR, EVO and VLC retain their existing testing limitations.

## Provider download fix (issue #3)

The local revision adds a PS5 socket-mode shim for the pinned curl 8.18.0 archive.
Its custom-open-socket path can emit curl 1 (Unsupported protocol) when the
nonblocking fcntl/readback operation fails, before any HTTP response or script
execution. The shim uses the console's SO_NBIO setter directly, retaining public
address validation, TLS verification, redirect controls and request limits.

The SDK-object regression reproduces the restricted-fcntl/readback failure and
checks the replacement, including a genuine setsockopt failure. The reporter's
exact firmware behavior is not reproduced on physical hardware. See
[PROVIDER_DOWNLOAD_0.5.31.md](PROVIDER_DOWNLOAD_0.5.31.md).

On the affected PS5, enable only 4KHDHub in Yoru's Repo, reopen the same title,
and save the complete provider error/log if it persists. Record the 0.5.31
source revision from build-info.json. Passing script download alone does not
guarantee a title has streams or all provider APIs are supported.

## Continue Watching with profiles (issue #4)

The candidate fix addresses [the missing Continue Watching report](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/4#issuecomment-6249838477).
A single unreadable Watch Progress row previously rejected the entire profile
snapshot, including all valid resume entries. Valid rows now load and persist
in the selected profile's cache. An incomplete snapshot cannot infer deletions,
advance the complete baseline or trigger a replacement upload. Logs report
profile index and readable/unreadable counts without content IDs or credentials.

The local HTTPS regression reproduces the missing entry with two profiles and
an unsupported short-duration row. It checks an in-progress title outside the
library, cached resume after restart/offline, isolation on switching back to the
primary profile, and preservation of pending writes/cache on partial snapshots.
This reproduces a concrete failure path, not the reporter's actual account data.

On PS5, select each existing profile, check Continue Watching against the phone
using that same profile and Nuvio Sync, resume a title, stop/restart, then switch
profiles and back. Check the correct title and position and no cross-profile
entries. If still missing, collect `progress:` and `library:` log lines without
account tokens/PINs. Device pairing can be used for real-account diagnosis;
password sharing is unnecessary.

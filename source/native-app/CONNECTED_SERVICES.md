# Connected Services in Nuvio Max'd

Connected Services is available in the v0.5.14 experimental release. It lets the PS5 ask
TorBox or Premiumize for a cached original file when an addon returns resolution
instructions instead of a ready-to-play link.

## Set it up

1. Open **Settings → Connected Services**.
2. Connect **TorBox** with the QR code on your phone or an API key. For
   **Premiumize**, enter the API key from your account page.
3. Alternatively, sign in to Nuvio and choose **Import connections from Nuvio
   Sync**. This copies supported connections from your primary Nuvio profile.
   If none appear, connect the service in regular Nuvio and sync it, or enter
   its API key here.
4. Turn **Resolve playable links** on. Set **Resolve with** to Automatic,
   TorBox or Premiumize. A preferred service must be connected.
5. Set **Instant playback** to the number of candidates to prepare while
   browsing: two by default, up to five, or Off. Sync import also copies this
   preference when your Nuvio profile includes it.
6. Reload addons after changing your AIOStreams configuration. Select a cached
   stream from the connected provider.

Automatic respects the provider named by the addon. For a bare torrent hash,
it uses TorBox if connected, then Premiumize. A specific preference restricts
client resolution to that service. Ready-to-play addon links keep using their
existing route; this preference doesn't rewrite those links.

## What to expect

While you browse the stream cards, the app prepares the focused candidate
first, then other candidates within your selected addon filter. A **Ready**
label means its original-file link is available. Selecting it reuses that link,
or joins an already-running request, rather than starting resolution again.
The app still needs to open the file and buffer video; Ready doesn't promise
zero loading time.

Prepared links stay in memory for five minutes. Expired links resolve again
when needed. Leaving the list cancels unfinished browsing work; selecting a
stream keeps its request alive. Disconnecting or changing services clears the
links. The app runs one background resolution at a time, with at most six
starts per minute and thirty per hour. Manual playback remains available when
that budget is reached.

Continue Watching uses the same resolver and short-lived cache. Near the end
of an episode, the app can prepare the next episode's matching stream before
the credits countdown. It keeps the existing quality rules and asks you to
choose if no match is available.

Resolution identifies the selected video or episode and requests its original
download link. It doesn't ask the service to convert the video. Current HDR
and audio output limits in the
[playback guide](PLAYBACK_QUALITY.md) still apply.

If a file isn't cached, the app asks you to choose a cached stream. It doesn't
start a new uncached provider transfer. Leave **Peer-to-peer torrents** off if
you want to avoid peer-to-peer playback of separate raw torrent results.

Sync import is a copy to this PS5, not continuous two-way synchronization.
Disconnecting here leaves the regular Nuvio connection alone. Signing out
clears imported credentials; connections made directly on this PS5 stay local.
API keys are saved in `/download0/nuvio/connected-services.json`, separately
from the ordinary settings. Don't share that file.

## Console checks before release

Use your own account and cached media. Host tests use dummy credentials and
simulated provider replies; they don't verify a paid provider account or PS5
playback.

- Pair TorBox using the QR code. Check cancellation, code expiry and reconnect.
- Enter each provider's API key; check that an invalid key produces a useful
  message and that connections survive restarting the app.
- Import the same connections from Nuvio Sync, verify the preferred service,
  and sign out. Imported connections should clear without changing the phone.
- Play a cached AIOStreams movie and an episode from a season pack with each
  provider. Confirm the selected filename, episode and advertised quality.
- Choose an uncached result and confirm it doesn't start a transfer or P2P.
- Cancel a stream while resolving, then choose another. An old request should
  never start playback after cancellation.
- Resume from Continue Watching and test Next Episode with the same provider.
- Wait on a stream card for Ready, then play it. Compare the launch time with
  Instant playback Off. Change addon filters, scroll quickly and leave the
  list while links are preparing; old work should stop.
- Leave a Ready card for more than five minutes and select it again. Its link
  should refresh. Change the preferred service and confirm Ready labels clear.
- Cancel a launch immediately after selecting a preparing card, then play a
  different stream. Only your new selection should launch.
- Play an existing direct 1080p H.264 link and a 4K HEVC link as regression checks.

Cloud Library and Premiumize QR pairing are later stages. This build hasn't
been promoted to the public updater feed.

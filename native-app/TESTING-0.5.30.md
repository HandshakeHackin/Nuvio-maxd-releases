# 0.5.30 profiles and provider testing

Experimental release. Install over the existing app while preserving app data. A backup
of the `/download0/nuvio` data folder is useful before migration testing.

## Changes

- Sign in with the existing Nuvio phone/web flow, then select Who's watching.
  Existing account profiles import. Create profile supports up to six profiles.
- Settings > Account & Services > Profiles reopens the picker. Stop playback
  before switching. Locked profiles require the account's PIN verification;
  failed profile/lock requests leave selection unchanged.
- Separate account/profile storage for history, Continue Watching, watched marks,
  source additions, provider switches, language preferences and connections.
  Display/lighting/install settings and media/art caches remain device-wide.
- Standard addon and NUVIO_JS plugin repository import for the selected profile.
  Source add/removal sync uses a per-profile persistent retry queue. Reload addons
  to retry pending changes. The complete remote set is read/merged before a push;
  DEX rows are preserved and unknown future formats prevent a destructive push.
- Respect independent uses-primary-addons/plugins flags. Inherited sets are
  read-only here. Change inheritance in Nuvio, then refresh/switch profiles.
- Watch Progress and watched-history RPCs use the selected profile. Manual
  watched/unwatched marks and completed playback sync, including offline retries.
- Legacy account data with a matching saved sync owner migrates once to that
  account's primary profile. Older data without an owner stays as a backup and
  is not assigned to a newly paired account.
  Guest and different-account history are kept separate. Older root files remain
  as migration backups; new writes go to scoped storage.

## PS5 / real-account checklist

1. Back up current data; upgrade without clearing it. Confirm the startup dialog
   and primary profile retain prior history, added sources and enabled providers.
2. Select an existing secondary Nuvio profile. Compare history, watched episodes,
   addons and plugin repositories with that exact profile on another device.
3. Create a new named profile. Confirm it appears on another Nuvio device after
   refresh and starts without another profile's local history/connections.
4. Add/remove one standard addon and one JS repository. Refresh the same profile
   on another device. Enable imported scrapers individually on this PS5: those
   individual switches are local per-profile, not part of repository cloud sync.
5. Test primary addon inheritance without primary plugin inheritance, then the
   reverse. Confirm the right source sets import and inherited sets cannot be edited.
6. Play, pause, stop and resume on the other device. Mark an episode watched and
   unmark it; confirm those marks sync without affecting a different profile.
   Include a source with custom episode IDs, specials and repeated viewing.
7. Interrupt networking during progress/source/history sync. Reconnect, reload
   addons/Watch Progress and check pending changes retry for their original profile.
8. Switch profiles while previous catalogs/history are loading; check no late
   responses populate the new profile. Stop a video immediately before switching.
9. Test a PIN-protected profile with wrong/correct PINs and a temporary lock-service
   failure. Close/reopen the app and verify it requests selection/PIN again.
10. Sign out and pair a different account; confirm no history, provider switches,
    cached account sources or connected credentials are imported from the prior account.

## Issue #3 provider test candidate

- Long provider menus now scroll with the selected row and show the visible range.
  Toggle with Cross; the menu remains open at that provider. Circle exits.
- Script downloads use the shared IPv4 setup. This is a candidate networking fix;
  the reporter's original error did not preserve the underlying cause.
- Failures now name the provider and HTTP status or curl error code. If a search
  still fails, collect `provider: script download failed` from the log, excluding
  personal data. Enable one provider first rather than every repository at once.
- Four sample scripts from the reported repositories were reachable through GitHub.
  This does not establish that every provider runs correctly on PS5.

## Limits

Real Nuvio account, PIN and PS5 testing remain outstanding. The app does not
modify Nuvio's hosted signup UI, provide local PIN management, or run Android
DEX/CS3 plugins. Backend profile creation relies on Nuvio's supported profile RPC;
unsupported/failed server operations report an error rather than pretending to
have created a synced profile. A network connection is needed to select/verify
an account profile at launch. Native remains the default playback engine; this
change makes no new HDR, audio or decoder claims.

## Local validation

- 69 production account/HTTPS/sync tests pass against a local TLS backend.
  Includes multi-profile source and watched-history import, creation limits,
  PIN failure handling, separate addon/plugin inheritance, account/profile
  cache isolation, stale context rejection, offline source/history retries,
  custom episode identifiers, metadata preservation and legacy migration.
- Production startup compilation, Settings categories, six navigation cases,
  preferences/release-note/diagnostic regressions pass.
- Native target compilation and image verification are recorded alongside the
  generated test artifacts. Hardware validation is not claimed.

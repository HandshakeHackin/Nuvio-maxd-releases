# Nuvio Max'd 1.0.0.15 experimental

**Unconfirmed USB detection fix for issue #20. PS5 tester confirmation required.**

- Try directory enumeration when USB mount metadata is unavailable; use known
  directory-entry types as picker hints when entry metadata fails.
- Distinguish missing mounts, access denial, metadata failures, enumeration
  errors and empty directories in privacy-safe per-mount diagnostics.
- Add scan cancellation and retain bounded scanning, strict import validation,
  profile isolation and private atomic saved copies.

Mounts checked: `/mnt/usb0` through `/mnt/usb7`. This does not establish an exFAT
problem or grant sandbox access. Import still rejects a candidate if its path
and identity cannot be validated. The crash avoidance from 1.0.0.14 is retained;
raw SDK dup/openat/fstatat calls are not reintroduced into USB import.

Keep saved user data and a previous installer. Fully close the app before
updating; do not overwrite a mounted FFPFSC image. Title ID PPSA99288,
contentVersion 07.000.015. Existing experimental playback features retain their
limits. See TESTING.txt for the console checklist. The issue stays open.

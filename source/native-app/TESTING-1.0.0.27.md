# Offline warning checks

1. An intact signed package should show **Verified release files** in Settings →
   App & Updates → Build authenticity & credits, without a startup warning.
2. On a separate test copy, change or remove `CREDITS.txt` or change branding in
   `sce_sys/param.json`. Startup/manual verification should show **Unverified build**.
3. Dismiss the notice and confirm normal browsing, playback and profile switching.
4. Restore the original files and recheck. Verification should pass again.
5. Check the notice and credits in your chosen menu language. Confirm the project
   link and contributor names remain visible.

The check is local and informational. It cannot identify a copier or distinguish
an unchanged copy. Do not edit your only installation or saved app data for testing.

# Nuvio Max'd 1.0.0.16 experimental

**Unconfirmed USB access fix for issue #20. PS5 confirmation required.**

- When direct USB scanning is denied, offer an optional bundled USB reader
  through the existing local payload loader. Max'd stays sandboxed.
- Restrict the reader to bounded USB playlist discovery and reading the selected
  file. It makes no file writes, opens no extra listener and changes no mounts
  or Max'd credentials. It is a privileged payload: the standard SDK startup
  prepares the helper process's own capabilities and syscall permissions.
- Keep validation, 0600 private atomic copies and profile isolation inside Max'd.
  Retain privacy-safe per-mount diagnostics and cancellation.

This addresses the reported access difference between FTP and the home-screen
app under Y2JB/ShadowMount 1.7beta12. It does not establish an exFAT problem or
promise the reader can access USB on every setup. The existing payload loader
must already be available on loopback port 9021. Failure keeps the previous copy.
The original USB crash avoidance is retained; raw SDK dup/*at calls are not used.

Keep saved user data and the previous installer. Fully close Max'd before updating;
do not overwrite a mounted FFPFSC. Title PPSA99288, contentVersion 07.000.016.
Source, package verification and test instructions are included. The new reader
payload needs catalog-maintainer review; publication is not a store deployment.
Issue #20 stays open pending console results.

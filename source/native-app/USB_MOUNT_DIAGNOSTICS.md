# Issue #20: USB visibility candidate, 1.0.0.15

The latest tester reply confirms 1.0.0.14 stopped the immediate crash on a PS5
Slim, FW 11.20, FFPFSC install. The exFAT drive and root .m3u remain undetected
on both rear ports. No 1.0.0.14 scan log is attached. The earlier screenshot
shows SYSTEM_ILLEGAL_FUNCTION_CALL without a fault address; the supplied
1.0.0.13 boot/runtime files show an unclean exit at 3840x2160, and app.log has
no USB trace. The zero-byte crash attachment returns HTTP 416. None of this
establishes an exFAT defect or identifies the remaining failed operation.

The previous scanner required lstat of a mount before opendir and silently
skipped entries if lstat failed. This candidate tries opendir when metadata is
unavailable. Known directory-entry types can provide picker hints on metadata
failure; DT_UNKNOWN and symlinks are rejected unless metadata identifies a
regular file or directory. Observed non-directory roots remain rejected.
Import remains strict: all component metadata, canonical path and opened-file
identity checks must succeed, with O_NOFOLLOW and bounded reading. If sandbox
metadata access remains denied, a displayed candidate can still be rejected
at import. This build cannot make an invisible or denied mount accessible.
It does not elevate privileges or add speculative mount aliases.

Each /mnt/usb0..7 probe logs its index, a fixed state, root metadata result and
errno, root open errno, directory/entry/candidate counts, metadata failure and
entry-type fallback counts, rejected entries, directory errors, enumeration
errno and budget status. There are no filenames, directory names, playlist
contents, account values or URLs in these diagnostics. Missing means both
lstat and opendir returned ENOENT; it does not prove a drive is disconnected
outside this app's namespace. Access-denied means EACCES/EPERM opening a root.
Successful zero-entry enumeration is distinct from open/enumeration errors.
Errno is cleared before every readdir so metadata errors cannot masquerade as
an enumeration failure. Error numbers follow the target platform's ABI.

Limits remain 8192 visited entries, 512 candidates, four folder levels,
128 MiB / 100000 channels and 16 saved USB sources per profile. Scan and import
are cancellable. Validation, private 0600 temporary files, fsync and atomic
rename, previous-copy retention and profile isolation are preserved. Path-based
identity checks retain the 1.0.0.14 limitation with concurrently changing media;
they are not an atomic directory-relative lookup. No raw SDK dup/*at calls are
introduced. A hint is never permission to bypass import validation.

Host fault injection covers missing, denied and empty mounts; root and entry
metadata failures; unknown entry types; readdir errors versus EOF; cancellation;
scan limits; and privacy. Existing tests retain playlist/HLS validation,
symlink/traversal rejection, private atomic replacement, unplugged reload and
profile isolation. These tests cannot establish PS5 filesystem compatibility.

# Issue #20 — USB native I/O candidate

The firmware 11.20 reporter crashes immediately on opening USB import, before
selecting a file. The supplied runtime state has no fault address or scan trace.
This is a candidate fix, not a console-confirmed diagnosis.

Pinned SDK libc.a defines dup, fstatat and openat in syscalls.o as direct
syscall instructions (numbers 0x29, 0x1ed, 0x1f3). The 1.0.0.13 executable
contains those local functions; open, opendir, readdir, lstat and fstat are native
imports. USB scanning called dup/fdopendir before returning any candidate and
fstatat while enumerating. Import also used openat. Host Linux tests accepted
these APIs and could not reproduce the PS5 illegal-function-call failure.

The candidate uses imported opendir/readdir/lstat and path-based open/fstat,
without descriptor duplication or *at calls. Scan depth/count/size caps remain.
Import rejects lexical traversal and observed symlinks in every path component,
checks canonical spelling, compares opened-file identity and rechecks component
identities to reject detectable replacement. This is not an atomic directory-
relative lookup: a concurrently modified removable filesystem cannot provide
all the same guarantees as openat. Only the user's selected USB playlist is
read; contents are validated before a private, atomic profile copy is replaced.

Regression tests abort on direct dup/fdopendir/fstatat/openat references and
exercise existing USB import behavior plus parent symlinks and missing mounts.
Scan/import stage logging omits playlist names, contents and stream URLs.
Physical PS5 retesting is required, especially scanning, nested playlists,
cancellation, unplugging, replacement and reload after USB removal.

Development version 1.0.0.14; previous published assets remain unchanged.

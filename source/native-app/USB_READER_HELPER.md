# Issue #20: scoped USB reader candidate (1.0.0.16)

The tester now identifies Y2JB, ShadowMount 1.7beta12 and home-screen launch.
Their FTP process can open usb0 and usb1 while Max'd is running, but Max'd gets
EPERM before enumeration. This demonstrates different process access. It does
not identify an exFAT/parser defect or prove which restriction produces EPERM.

ShadowMount tag 1.7beta12 (cef1579a496240f14d0aad28397b02f283a25b7a)
mounts USB aliases into a title sandbox in src/sm_fakelib.c. Its files API lists
entries but has no general read-file route. File visibility from FTP does not
establish the title's permission to enumerate that alias. References:
https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/20#issuecomment-6776863458
https://github.com/drakmor/ShadowMountPlus/blob/1.7beta12/src/sm_fakelib.c
https://github.com/drakmor/ShadowMountPlus/blob/1.7beta12/docs/openapi.yaml

## Candidate behavior

Direct scanning remains first. When a root is denied and no files are found,
the app offers Cancel / Use USB reader helper. Only the user's explicit choice
starts the bundled /app0/nuvio-usb-reader.elf via the existing loopback payload
loader on 127.0.0.1:9021. No payload is downloaded and no external listener is
created. The title is not elevated. USB request handlers never change credentials or
mounts and never write files. The standard SDK payload startup does prepare
the helper process's own capability/attribute bits and syscall-address range
through kernel access before main; this is a privileged payload, not ordinary
sandboxed code. It uses the same pinned SDK runtime as the existing updater. If the loader is unavailable or
the reader is also denied, it fails with a message and retains existing data.
The code does not start a new jailbreak or change ShadowMount configuration.

Each helper handles one request over the loader's inherited stdin/stdout stream
and exits. This stream is IPC, not a log: it carries a bounded manifest or the
selected playlist bytes. Diagnostic lines contain only mount indices, operation
states/counts and errors. No filenames, body data, credentials or URLs are logged.
The helper has no network client and accepts no URLs, destination/profile paths
or write/mount commands. Requests must name protocol 1 and title PPSA99288.

Scan is fixed to /mnt/usb0..7: 8192 visited entries, 512 candidates, four folder
levels. Read accepts an index in 0..7 and a relative .m3u/.m3u8 path of at most
1024 bytes/five components, rejecting dot traversal, empty components, NUL and
backslash. It requires regular-file metadata, canonical spelling, O_NOFOLLOW,
opened-file identity and component rechecks. Existing path-based TOCTOU limits
with concurrently modified media remain; this is not an atomic openat lookup.
Neither the title's USB code nor the reader references raw dup/*at stubs.

The app rejects oversized/truncated/malformed replies and duplicate or excessive
manifest entries. Buffered reads preserve binary data arriving with a JSON
header. Payload size is capped at 16 MiB (including the existing larger updater);
USB operations use a 30-second channel
deadline with existing bounded socket waits. Cancellation is checked between
I/O calls; a blocked platform call can defer cancellation until its socket wait
expires. The helper also watches disconnects and requests cancellation after
30 seconds. Filesystem-call latency remains controlled by the operating system.

Only a selected file's body (1..128 MiB) returns to the title. The app applies
the existing channel-playlist validation/100000-channel cap and cancellation,
then writes its own 0600 temporary file, fsyncs and atomically renames it inside
the captured active profile. The helper never sees profile paths or IDs.
Invalid data, disconnection and cancellation retain the previous working copy.
The existing 16 USB sources/profile limit and stale-profile result guards remain.

## Evidence and limits

Host tests run the actual helper via inherited pipes, and test the actual
client/channel with fragmented and coalesced data. They cover rejected titles,
commands, roots/traversal, symlinks, oversized files, malformed/oversized replies,
512/513 candidate boundaries, disconnect/cancellation, HLS/invalid playlists,
private permissions, atomic replacement and profile isolation. Existing USB and
updater regressions remain required. Native compile/import and package checks
must pass before publication. A new reader payload requires store-maintainer
review; helper approval files are outside this app's catalog submission scope.

This is an unconfirmed candidate. Host tests cannot establish loader availability,
payload USB access or correct operation on the tester's firmware 11.20 setup.

Runtime audit: SDK v0.45 crt/crt.c invokes __kernel_init and __patch_init before
main. crt/patch.c sets the current helper process's capability/attribute bits
and syscall-address range. It does not change Max'd's credentials. Those SDK
functions remain linked into the payload. Host protocol tests do not validate
these kernel operations on the tester's firmware. See
https://github.com/ps5-payload-dev/sdk/blob/v0.45/crt/patch.c

# libass 0.17.5 input hardening

The maintenance overlay retains libass 0.17.5 and applies three upstream ISC-licensed commits, in order:

- [d4fa71042524](https://github.com/libass/libass/commit/d4fa710425240cc1c1fcee1815041263bf83c745): reserve the terminator on empty codepage-converted input.
- [1f0f8e344b30](https://github.com/libass/libass/commit/1f0f8e344b30b27123bf9801d337448f68e18449): reject size overflow when growing the conversion buffer.
- [fe25f50686b1](https://github.com/libass/libass/commit/fe25f50686b1a6babef2f9b703abead0e97e0fec): reject overflowing ass_read_memory input sizes.

Source and patch checksums, provenance and target library hashes are recorded in dependencies.json. The source archive checksum is unchanged. The recipe rebuilds libass when this patch set changes, and the library validator checks the patch and binary hashes. No other library version is upgraded.

Run `tests/libass_hardening.sh /path/to/libass-0.17.5.tar.xz` for the real-library AddressSanitizer regression. The test covers empty codepage input and SIZE_MAX lengths. Leak checking is disabled by default for restricted hosts; address checking remains enabled.

Original copyright and ISC terms are retained in LICENSE and in the upstream sources.

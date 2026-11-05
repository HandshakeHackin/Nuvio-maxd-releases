# Max’d integration changes

The upstream release remains EVO Player 0.13.0, commit
`2db44b5294657b12bf91666154a05f029ddb5aa2`.

`patches/0001-hevc-base-layer-only.patch` rejects nonzero HEVC `nuh_layer_id`
values and truncated NAL headers before the PS5 decoder receives them.
This lets HDR10-compatible Dolby Vision profile 7 streams retain base-layer
pictures while omitting enhancement pictures. It does not implement Dolby
Vision output or convert profile 5 to HDR10.

The six-bit layer check follows the HEVC NAL header layout, reviewed alongside
[Stremio Plus 0.6.00](https://github.com/LoZazaMastro/Stremio-Plus/releases/tag/0.6.00).
VLC’s current PS5 module already has this check. Native now follows the same rule.

UPSTREAM.json retains original file hashes and separately records patch and
patched-file hashes. The validator reverses the patch and checks the original
release bytes. The original GPL-3.0 notice remains in LICENSE.

## 2026-11-05 upstream maintenance review

Reviewed v0.13.5 (`f6e43e6561796630673f1e20eb1eb67d6428514c`)
against v0.13.0 for the integrated decoder, clock and playback files.
`pp_clock.c`, `pp_playback.c`, `evo_vdec_native.h` and `evo_vdec.h` are
unchanged. Decoder changes introduce profile 7/8 Dolby Vision per-shot peak
mapping and depend on new RPU/map-only helpers and matching presentation
support. They do not repair the current base-layer integration. They are not
imported in this maintenance update; the tested HEVC base-layer filter and
original provenance remain intact. A complete tone-mapping integration needs
its own renderer and console validation.

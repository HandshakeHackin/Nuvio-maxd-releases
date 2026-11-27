# Upstream update: Bnyro/GermanProviders

> This file is generated automatically. The Nuvio JavaScript providers are NOT overwritten automatically.

## Commits

- 3fe86dd hdfilme: migrate to new website layout

## Changed Kotlin files

### hdfilme

- M HDFilme/src/main/kotlin/com/bnyro/HDFilme.kt
- M HDFilme/src/main/kotlin/com/bnyro/HDFilmeExtractors.kt
- M HDFilme/src/main/kotlin/com/bnyro/HDFilmePlugin.kt

## Nuvio files to review

- src/hdfilme/index.js → likely review target

## Upstream comparison

- Previous: 41e7d9f2283f35aa3ff749dd9f8e73c4e28ea625
- Current: 3fe86ddf4dc789392cf53f4c66716c17c4fb0f36
- Compare: https://github.com/Bnyro/GermanProviders/compare/41e7d9f2283f35aa3ff749dd9f8e73c4e28ea625...3fe86ddf4dc789392cf53f4c66716c17c4fb0f36

## Porting procedure

1. Review the upstream Kotlin diff.
2. Check the corresponding src/<provider>/index.js.
3. Port only the functional changes that apply to Nuvio.
4. Run `npm ci && npm run build && npm test`.
5. Test the affected provider in Nuvio before merging.

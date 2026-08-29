# Git-baseret firmwareversionering

Buildet bruger samme model som frysetørrer-firmwaren. Kun annoterede Git-tags,
der matcher `vMAJOR.MINOR.PATCH`, er releases. Et build på tagget `v2.2.0` får
version `2.2.0`; commits efter tagget får eksempelvis
`2.2.0-dev.3+g1a2b3c4`, og lokale ændringer tilføjer `.dirty`.

Uden et gyldigt tag er versionen eksplicit `0.0.0-unknown`. Buildnummeret er
antal commits, og Git-hash samt UTC-buildtid indlejres i firmwaren.

Efter build ligger firmware, SHA-256 og `firmware-manifest.json` i
`.build-artifacts/esp32-s3-devkitc-1-16mb-psram/`.

Releaseflow:

1. Commit ændringerne og kontrollér et rent working tree.
2. Opret et annoteret tag, fx `git tag -a v2.2.0 -m "Release v2.2.0"`.
3. Byg firmwaren og arkivér `.bin`, `.sha256` og manifest sammen.

Der oprettes aldrig automatisk commits eller tags.

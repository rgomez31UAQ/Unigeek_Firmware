# Website & Content Reference

## Website Content Sync

The website reads content directly from the repo at build time:
- `knowledge/*.md`          → feature detail pages (`website/content/features/index.js`)
- `release-notes/*.md`      → release notes page (`website/content/releases/index.js`)
- `README.md`               → source of truth for feature catalog

When any of these change, the following must also be updated:

| Changed file | Update required |
|---|---|
| `README.md` (features section) | `website/content/features/catalog.js` — add/remove/rename entries, fix summaries, fix categories |
| `knowledge/*.md` (new file)    | `catalog.js` — add entry with `hasDetail: true` and correct slug matching the filename |
| `knowledge/*.md` (removed)     | `catalog.js` — set `hasDetail: false` for that slug |
| `release-notes/*.md` (new)     | No action needed — read automatically at build time |

Do not wait for the user to ask. When editing README.md or knowledge/ files, scan catalog.js immediately and apply any needed corrections.

---

## Crediting References

When porting or referencing features from another repository, update the "Thanks To"
section in README.md with the specific features taken from that repo.
Format: list the repo link and author, then sub-bullets for each feature referenced.

    - [RepoName](url) by author
      - Feature A
      - Feature B

---

## Migration from Evil-M5Project

    Reference: ../Evil-M5Project/Evil-Cardputer*.ino
    When asked to "learn from evil m5", read the relevant Evil-Cardputer .ino files for reference.

    Not yet ported:
      - Karma Attack         → WiFi (rogue AP that responds to probe requests)
      - Full Network Analysis → WiFi (ARP scan + port scan on connected network)

---

## Release Process

    Binary downloads: https://github.com/lshaf/unigeek/releases
    SD card content: copy sdcard/ to SD root, or use built-in Download menu in firmware

    sdcard/manifest/sdcard.txt must be updated when files are added or removed from sdcard/
    sdcard/manifest/ir/categories.txt lists IR categories (folder|Display Name format)
    sdcard/manifest/ir/cat_{folder}.txt lists files per IR category (from Flipper-IRDB repo)

### Build Gates (explicit permission required)

NEVER add a board to any of the following unless the user explicitly asks:
- `build_all.sh` ENVS list
- `platformio.ini` default_envs
- `.github/workflows/release.yml` matrix
- `website/content/boards.js`

Create board files first; wire into build/release/website only on explicit instruction.
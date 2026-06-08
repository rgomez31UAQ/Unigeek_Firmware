# Wikipedia

Browse Wikipedia straight from the device over WiFi — search, open random articles, see what happened on this day in history, and keep a cross-language favorites list. Articles are fetched as plain text, cached to storage per language, and opened in the shared file viewer, so you can read them again later with no connection.

There are two ways in:

- **WiFi → Network → Wikipedia** — the full experience. Search, Random, and On This Day all **download** fresh articles over WiFi and cache them. Requires a WiFi connection and available storage.
- **Utility → Wikipedia** — **read-only**. No WiFi and no downloading; you only browse articles already cached on storage (Favorites, All Cached, Search Cached, Language).

## Menu

| Item | What it does |
|---|---|
| Search | Free-text query against the current language edition; results paged 50 at a time |
| Random Article | Open a random article from the current edition |
| On This Day | Historical events for today's date (from the device RTC), each opening its linked article |
| Search Cached | Filter already-cached articles (current language) by text — works offline |
| All Cached | List every cached article in the current language — works offline |
| Favorites | Pinned articles across **all** languages |
| Language | Pick the Wikipedia edition (persisted) |

## Searching and reading

Pick **Search**, type a query, and you get a paged list of matching titles. Long lists show **Prev** / **Next** rows to walk through pages (50 results per page, via the MediaWiki `sroffset` cursor).

Selecting a result fetches the full article text (up to 100 KB) and caches it under `/unigeek/wikipedia/<lang>/<title>.txt`, then opens it in the standard file viewer, which word-wraps `.txt` content. Because the text is cached, re-opening it later through **All Cached** / **Search Cached** needs no network.

> [!note]
> Article text is plain prose only — no images, tables, or infoboxes. It's pulled from the MediaWiki TextExtracts API (`explaintext`), so what you read is the readable body of the page.

## Long-press context menu

Long-press (hold ~0.8 s) any result or cached-article row to open a per-item menu:

| Action | Effect |
|---|---|
| Open | Read the article (same as a normal tap) |
| Favorite / Unfavorite | Toggle the article in your favorites list |
| Share QR | Show a QR code linking to `https://<lang>.wikipedia.org/wiki/<title>` so a phone can open the real page |

Favorites are stored in `/unigeek/wikipedia/favorites.txt` and span every language, so the **Favorites** list mixes editions freely.

## Languages

The language picker offers **21 Latin-script editions**: English, Indonesia, Malay, Spanish, Portuguese, French, German, Italian, Dutch, Catalan, Polish, Czech, Slovak, Croatian, Hungarian, Romanian, Turkish, Swahili, Filipino, Esperanto, and Latin. Your choice persists across reboots (config key `wiki_lang`, default `en`).

> [!note]
> Only Latin-script editions are listed because the TFT renders ASCII. Article text is UTF-8 decoded and accented Latin letters are transliterated to a base letter (é → e, ñ → n, ß → ss); characters with no reasonable Latin base render as `?`. Cyrillic, CJK, Arabic, Thai and other non-Latin scripts can't be displayed, so those editions are intentionally omitted.

Each language keeps its own cache directory, so switching languages gives you a separate set of cached and searchable articles (favorites remain shared).

## Achievements

| Achievement | Tier | Unlock |
|---|---|---|
| Encyclopedist | Bronze | Search Wikipedia for the first time |
| First Read | Bronze | Read a Wikipedia article |
| Bookworm | Silver | Read 10 Wikipedia articles |
| Lucky Dip | Bronze | Open a random Wikipedia article |
| History Buff | Bronze | View Wikipedia On This Day events |
| Bookmarked | Bronze | Favorite a Wikipedia article |
| Pass It On | Bronze | Share a Wikipedia article as a QR code |
| Polyglot | Silver | Read Wikipedia articles in two languages |

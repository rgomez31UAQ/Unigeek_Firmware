---
name: patch-website
description: Redeploy the website only (no firmware build) by tagging <version>-web-<n>
user-invocable: true
---

# Patch Website Skill

Ship a **website-only** redeploy without cutting a new firmware release. Pushes a
`<version>-web-<n>` tag that triggers `.github/workflows/website.yml` (build-website →
deploy-pages, no firmware matrix). Use this after editing anything under `website/`,
`knowledge/`, `release-notes/`, or `README.md` features when the firmware itself hasn't
changed.

Usage: `/patch-website` (no argument — the version is derived automatically).

This is **not** `/release`. It builds nothing on the firmware side, writes no release
notes, touches no `_boards.json`, and does not publish to M5Burner. If the firmware
changed, use `/release` instead.

## How the tag is derived

- **Base version** = the latest real release tag, ignoring any `-web-*` tags:
  ```bash
  git tag --sort=-v:refname | grep -E '^[0-9]+\.[0-9]+(\.[0-9]+)?$' | head -1
  ```
  > [!warn]
  > You MUST filter out `-web-*` tags here. Git's version sort puts `1.8.2-web-1`
  > *above* `1.8.2`, so a bare `git tag --sort=-v:refname | head -1` would return the
  > previous patch tag, not the firmware version.

- **Next counter** = highest existing `<base>-web-N` + 1, or `1` if none exist yet:
  ```bash
  base=$(git tag --sort=-v:refname | grep -E '^[0-9]+\.[0-9]+(\.[0-9]+)?$' | head -1)
  last=$(git tag --list "${base}-web-*" | sed -E 's/.*-web-//' | sort -n | tail -1)
  next=$(( ${last:-0} + 1 ))
  echo "${base}-web-${next}"
  ```
  So the first patch on `1.8.2` is `1.8.2-web-1`; the next is `1.8.2-web-2`, and so on.
  When the firmware moves to a new release (e.g. `1.8.3`), the counter resets to `1`
  because no `1.8.3-web-*` tag exists yet.

The `website.yml` workflow strips the `-web-<n>` suffix before setting
`NEXT_PUBLIC_FIRMWARE_VERSION`, so the deployed site still shows / flashes the real
firmware version (`1.8.2`), not the patch tag.

## Steps

1. **Derive the tag** using the two commands above. Report the computed
   `<version>-web-<n>` to the user.

2. **Verify the website build — this gate is mandatory and runs before any commit.**
   ```bash
   cd website && npm run build
   ```
   - If it fails with `next: command not found`, run `npm install` first
     (`cd website && npm install && npm run build`).
   - If the build reports **any** error, **stop and show the output**. Do not commit,
     do not tag, do not push. A red build must never be deployed.
   - Spot-check that any `knowledge/*.md` edited this cycle still renders (the catalog
     row exists and `hasDetail` matches — slug = filename without `.md`).

3. **Stage and commit pending changes** (only after the build is green):
   - `git status` to see what changed. Stage the website-related changes.
   - Commit with a short, descriptive message and a random leading emoji, e.g.
     `🌐 web: <what changed>`. Do not add any co-author trailer.
   - If the working tree is already clean, there is nothing to commit — this becomes a
     pure redeploy of the current `HEAD`. That's valid; just say so and continue.

4. **Show the plan and wait for explicit approval** before the push:
   - the tag to be created (`<version>-web-<n>`),
   - the commit (if any) that the tag will point at,
   - confirmation that `npm run build` passed.
   Do not push until the user confirms (per the repo git policy — no push without an
   explicit go-ahead).

5. **On approval, push the commit then the tag:**
   ```bash
   git push origin main
   git tag <version>-web-<n>
   git push origin <version>-web-<n>
   ```
   The tag push triggers `website.yml`. Lightweight (non-annotated) tags are fine here —
   these are deploy markers, not releases.

6. **Report** the pushed tag and remind the user they can watch the run under the
   "Update Website" workflow in GitHub Actions.

## Notes

- The Cloudflare proxy host the flasher fetches from lives in
  `website/content/meta.js` (`FIRMWARE_PROXY`). A website deploy goes live with whatever
  that points at — if it was just changed, make sure the subdomain is actually bound.
- See [[website-release]] (Obsidian: `project/unigeek/website-release.md`) → "CI
  workflows" for how `release.yml` and `website.yml` divide responsibility.
# Publishing this wiki

*(Developer doc.)* The pages you're reading live as Markdown in the **`wiki/` folder of the main repo**.
That folder is the source of truth — edit it in a normal pull request like any other code. GitHub's
**Wiki tab** (`github.com/coreflake1/NebulaOS-guppyscreen/wiki`) is a *separate* git repository
(`...NebulaOS-guppyscreen.wiki.git`), so the files have to be copied across to actually appear there.
The workflow below publishes to this repository's own wiki (the action takes the repository from the
workflow context).

This page explains how that copy happens and the conventions to follow so it doesn't break.

## Why it's set up this way

- **Source of truth = `wiki/` in the main repo.** Edits go through PR review, ship with the code change
  that motivated them, and are versioned alongside the release.
- **The Wiki tab is a publish target**, not where you edit. Don't edit pages directly in the GitHub Wiki
  UI — those changes live only in the `.wiki.git` repo and get overwritten on the next sync.

## Link & filename conventions (important)

The GitHub Wiki maps a file like `Calibration-Explained.md` to the page URL `…/wiki/Calibration-Explained`.

- **Link to other pages without the `.md` and without a path:** `[text](Calibration-Explained)`, not
  `[text](Calibration-Explained.md)` or `[text](wiki/Calibration-Explained.md)`. The `.md`/path form
  works when browsing the repo but **404s on the Wiki tab** — and the Wiki tab is the canonical home.
- **`_Sidebar.md`** is the left-hand navigation. **`Home.md`** is the wiki landing page.
- Use dashes in filenames; spaces in the page title (the `# H1`) are fine.

## Publishing — CI (enabled; this is how it actually happens)

`.github/workflows/wiki.yml` auto-syncs `wiki/` to the live GitHub Wiki on every push to **`main`**
that touches `wiki/**`:

```yaml
name: Publish wiki
on:
  push:
    branches: [main]
    paths: ['wiki/**']
permissions:
  contents: write
jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Sync wiki/ to the GitHub Wiki
        uses: Andrew-Chen-Wang/github-wiki-action@v4
        with:
          path: wiki/
          token: ${{ secrets.GITHUB_TOKEN }}
```

**Wiki edits on any other branch don't reach the public Wiki tab until they are merged into
`main`.** Nothing needs to be run by hand.

## Publishing — manual method (fallback only)

Only needed if the CI workflow is ever broken/disabled, or for a one-off sync outside of a `main`
push:

```sh
# one-time: clone the wiki repo (must have at least one page created via the Wiki tab first —
# a genuinely empty GitHub wiki's .wiki.git doesn't exist yet and this clone will 404 until then;
# confirmed live 2026-08-15, the same way the CI job above failed the same way on 2026-08-14)
git clone https://github.com/coreflake1/NebulaOS-guppyscreen.wiki.git /tmp/nebulaos-guppyscreen-wiki

# each publish:
cp wiki/*.md /tmp/nebulaos-guppyscreen-wiki/
cd /tmp/nebulaos-guppyscreen-wiki
git add -A && git commit -m "Sync wiki from main@<short-sha>" && git push
```

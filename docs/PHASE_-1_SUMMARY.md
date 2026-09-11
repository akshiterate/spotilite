# Phase -1 Summary — Git + GitHub Smoke Test

## Goal

Prove git + GitHub works before any code is written (plans.md Phase -1).

## What was done

- Verified repo state: branch `main`, `origin`
  `https://github.com/akshiterate/spotilite.git` (matches canonical remote
  in plans.md 1.18), `user.name`/`user.email` configured locally.
- Updated `README.md` with repo title (`Spotify-lite`) + one-line goal.
- Created `.gitignore` ignoring `build/`, `target/`, `.env`, `*.token`,
  `*token*`, `*credential*`, `cache/`, `auth/`.
- Created `docs/` stubs: `API.md` (no C ABI yet — Phase 2), `DECISIONS.md`.
- Versioned `plans.md` in the repo per explicit user request (exception to
  the default Phase -1 file scope, recorded in `docs/DECISIONS.md`).
- No code. No CMake/Cargo changes. No secrets. No build required for
  this phase (build system is Phase 0 scope; no `CMakeLists.txt` exists yet).

## librespot rev

N/A — pinned in Phase 0 per plans.md.

## How to test

1. `git log --oneline -3`
2. `git remote -v`
3. `git ls-remote origin`
4. `git status --short` (expect clean)
5. Open `https://github.com/akshiterate/spotilite` in a browser and
   verify the commit is visible and `README.md` renders.

Expected result: all commands succeed, working tree clean, commit visible
on GitHub, README renders.

## Known issues

- None. Push requires configured GitHub credentials; if push fails the
  reason is reported per plans.md 1.18 (no remotes rewritten, no history
  rewritten, no credentials stored in the repo).

## Version control

Commit: `phase--1: init repo + README smoke test`
Hash: `a587a0590c8ef1940934b125d691339af2053935`

`git log --oneline -5` (at commit time):

```text
a587a05 phase--1: init repo + README smoke test
56faf1e Add blank README
```

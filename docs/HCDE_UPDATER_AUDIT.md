# HCDE Updater / Release-page Audit

Roadmap board item: **#18** ("check over the updater"). The "updater" is in
practice a **changelog viewer** — there is no auto-download or install
pipeline. This audit captures the real behaviour, the surface area, and the
risks worth tracking before any future change in this area.

Files audited:

- `src/widgets/releasepage.cpp` / `releasepage.h` — the launcher tab.
- `src/d_iwad.cpp` (around line 855) — wires `notifyNewRelease` into the
  startup flow via the `i_display_new_release` / `i_is_new_release`
  pseudo-CVARs.
- `src/widgets/launcherwindow.cpp` — gates display of the release notes
  tab on `info.isNewRelease && info.notifyNewRelease`.

## What it actually does

1. **Live changelog fetch (Windows only).** `FetchGitHubChangelog()`
   creates a temp `.ps1` file and calls
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden`
   to query
   `https://api.github.com/repos/bokoxthexchocobo/HCDE/releases?per_page=8`,
   pretty-prints the latest 3 non-draft / non-prerelease releases, and
   returns the captured stdout.
2. **Packaged fallback.** Reads `meta.xml` (the AppStream metadata) out of
   `hcde.pk3` and parses up to 3 `<release>` blocks via rapidxml.
3. **"Show this on next startup" toggle.** A checkbox driven by the
   `notifyNewRelease` field in `FStartupSelectionInfo`. When unchecked the
   release tab is skipped on the next launch (gated through
   `i_display_new_release`).

There is no version comparison, no download path, no install path, and no
tampering with installed files. The "updater" descriptor in the roadmap
is a misnomer; this is purely informational.

## Risks worth tracking

| ID | Risk | Severity | Notes |
| --- | --- | --- | --- |
| U1 | PowerShell shell-out for HTTPS fetch | Medium | Spawning `powershell.exe` from a launcher startup path is an AV-friendliness footgun and adds 100-500ms of cold-start cost. A native HTTP client (the engine already links libcurl-equivalents elsewhere) would be much smaller and equally cross-platform. |
| U2 | Temp `.ps1` file race | Low | `CreateTempScriptPath()` uses `GetTempFileNameW` then `MoveFileExW` to rename to `.ps1`. The window between create and rename is narrow but technically observable; an attacker with local write to `%TEMP%` could substitute content. Mitigation: write the file content with exclusive create flags before the rename, or skip the rename entirely and just call `powershell.exe -Command` with the script inline. |
| U3 | Hard-coded repo path | Medium | `https://api.github.com/repos/bokoxthexchocobo/HCDE/releases` is baked into the script. Any rename / fork move silently breaks the live tab and falls back to the packaged metadata. Move to a `version.h` constant or a build-time substitution. |
| U4 | GitHub API rate limit | Low | Anonymous GitHub API caps at 60 req/hour/IP. Rapid relaunches (or LAN parties) can exhaust this. Cache the last successful response on disk with a TTL of a few hours. |
| U5 | TimeoutSec 6 may swallow legitimate fetches | Low | Slow networks silently fall back to the packaged changelog. Operationally fine; document that the tab being "stale" is normal for offline / firewalled hosts. |
| U6 | Windows-only live fetch | Medium | `FetchGitHubChangelog` is `#ifdef _WIN32`. macOS and Linux launchers always show the packaged metadata. A small libcurl-based fetcher would unify the code path. |
| U7 | Parser swallows failures silently | Low | Both rapidxml parsing and the PS fetch return `NOTES_FAIL` / empty strings without surfacing the cause. Add a log breadcrumb so the user can see "fetch failed: timeout" vs "fetch failed: no metadata.xml" rather than just an empty tab. |
| U8 | "Updater" naming is misleading | Documentation | Rename the board item or rename the file/UI tab to "Changelog". The current name implies functionality that does not exist. |

## Non-issues

- **`_OpenReleaseNotes()` allocation.** `calloc(data.size()+1, 1)` plus
  `strncpy(notes, data.string(), data.size())` is correct; the calloc-zeroed
  trailing byte is the null terminator.
- **`notifyNewRelease` plumbing.** The checkbox correctly drives the
  startup gate via `i_display_new_release` and is honoured in
  `LauncherWindow`. No bug here.
- **Description parser HTML subset.** The hand-rolled `_ParseReleaseNotes`
  is fine for the tightly-scoped AppStream metadata grammar; broader HTML
  is not relevant here because the input is always the engine's own
  metadata.

## Recommended follow-ups

A future change can collapse U1, U2, U3, U6 into a single small refactor:

1. Replace `FetchGitHubChangelog()` with a libcurl-based fetcher that
   compiles on all platforms.
2. Move the repo URL to a constant in `version.h` so renames are a
   one-line change.
3. Add a small on-disk cache (e.g.
   `<config dir>/hcde-changelog-cache.json`) with a TTL of 6h.
4. Surface fetch failures via a single line in the tab ("Live changelog
   unavailable, showing packaged history.").

### Libcurl replacement plan (2026-05-28)

The first abstraction has landed in `src/widgets/releasepage.cpp`:
`FetchGitHubChangelogNative()` returns a typed status and empty text today,
then falls back to the existing Windows PowerShell path / packaged metadata.
The safest replacement is to fill that helper with read-only native HTTP while
preserving the current UX:

1. **HTTP wrapper.** Add a tiny launcher-side HTTP GET wrapper using the same
   libcurl (or platform HTTP abstraction) already acceptable elsewhere in the
   project. Hardcode method `GET`, no redirects beyond HTTPS-to-HTTPS, timeout
   6 seconds, response cap 256 KiB.
2. **Repo constant.** Move
   `bokoxthexchocobo/HCDE` into a build/version constant such as
   `HCDE_GITHUB_REPO`, then build the URL as
   `https://api.github.com/repos/<repo>/releases?per_page=8`.
3. **Cache.** Store the last good JSON response at
   `<config dir>/hcde-changelog-cache.json` with a timestamp sidecar or small
   wrapper object. TTL: 6 hours. Offline startup reads cache first, then
   packaged `meta.xml`.
4. **Parser.** Keep the current "latest three stable releases" logic, but move
   it from a PowerShell script to C++ JSON parsing. If no JSON parser is already
   linked in this launcher target, use a minimal rapidjson-style parse or a
   narrow hand parser for the fields already consumed (`name`, `tag_name`,
   `body`, `draft`, `prerelease`, `published_at`).
5. **Failure breadcrumb.** Return an enum status (`LiveOk`, `CacheOk`,
   `PackagedFallback`, `NetworkTimeout`, `JsonError`, `NoReleases`) so the tab
   can show one quiet line explaining why the packaged changelog is displayed.
6. **No updater scope creep.** The helper must not download assets, verify
   installers, or alter installed files. This remains a changelog viewer.

This is **not** prerequisite work for any other roadmap item; the current
behaviour is functional. The audit's purpose is to make the trade-offs
visible before someone "fixes" U1 by widening the surface.

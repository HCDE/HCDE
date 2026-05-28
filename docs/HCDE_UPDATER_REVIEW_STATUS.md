# HCDE Updater Review Status

This note is the Step 18 execution checkpoint for the roadmap integration plan.
Updater work is release tooling and must stay outside engine runtime/gameplay
packet lanes.

## Current Surfaces

- **Release checksums**: `tools/package-hcde-prerelease.ps1` uploads
  `SHA256SUMS.txt` with runtime, compat, and optional symbols assets.
- **Verifier**: `tools/verify-hcde-updater.ps1` validates trusted release URLs,
  selects the runtime Windows x64 zip, downloads `SHA256SUMS.txt`, and checks the
  selected asset hash before extraction.
- **Archive safety**: updater simulation rejects absolute paths, drive-qualified
  paths, `.`/`..` path segments, and colon-bearing entry names before extraction.
- **Payload contract**: runtime archives must contain both `hcde.exe` and
  `hcdeserv.exe` at the payload root.
- **Recovery paths**: verifier still covers backup creation, transient file-lock
  retry, rollback, stale lock cleanup, and status-file reporting.

## Runtime Smoke Check

```powershell
powershell -ExecutionPolicy Bypass -File tools/verify-hcde-updater.ps1
```

The command contacts the latest GitHub release, so it requires network access and
a release that includes `SHA256SUMS.txt`.

For local-only updater simulation checks without contacting GitHub:

```powershell
powershell -ExecutionPolicy Bypass -File tools/verify-hcde-updater.ps1 -SkipLatestRelease
```

## Step 18 Rules

- Keep updater validation in tooling scripts; do not add runtime gameplay code.
- Verify release hashes before expanding downloaded archives.
- Reject unsafe archive paths before extraction.
- Preserve rollback/status logging so failed updates leave a diagnosable result.

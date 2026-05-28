# Windows Launcher Updater

HCDE includes a Windows updater flow in the launcher `About` tab.

Scope:

- Windows x64 runtime package updates
- Source: latest GitHub release from `bokoxthexchocobo/HCDE`
- Install model: in-place update with automatic backup and rollback path

Launcher workflow:

1. Open `About`.
2. Click `Check for updates`.
3. If a newer version is found, click `Install update`.
4. HCDE exits, updater applies the package, then restarts HCDE.

UI controls:

- `Check for updates`
- `Install update`
- `Open updater backups`
- `Open last updater log`
- bottom-bar notice button appears when an update is available or updater attention is required

Release/API behavior:

- checks `https://api.github.com/repos/bokoxthexchocobo/HCDE/releases/latest`
- compares latest tag against local `VERSIONSTR`
- prefers runtime zips matching `HCDE-<version>-windows-x64.zip`
- de-prioritizes non-runtime zips (`symbols`, `pdb`, `debug`, `source`, `src`)

Update safety guards:

- download URL must be `https`
- allowed hosts: `github.com`, `api.github.com`, `objects.githubusercontent.com`, `release-assets.githubusercontent.com`
- package must contain `hcde.exe` at payload root (or single top-level folder payload root)
- refuses update if install target resolves to filesystem root

Updater files written under `<install dir>\backups`:

- `hcde-update.lock` (active updater lock)
- `hcde-update-last-status.txt` (last run summary)
- `hcde-update-<version>-<timestamp>.log` (per-run log)
- `hcde-backup-<version>-<timestamp>.zip` (pre-update backup)

Lock/cleanup behavior:

- lock prevents concurrent updater runs
- lock is treated as stale after `120` minutes
- stale lock is auto-cleared before a new run

Rollback and recovery:

1. If update fails and rollback succeeds, relaunch HCDE and inspect `hcde-update-last-status.txt`.
2. If update fails and rollback fails, restore manually from the newest `hcde-backup-*.zip` in `backups`.
3. If updater appears stuck, close all HCDE processes and re-open launcher; stale lock cleanup runs automatically.

Manual validation script (repo utility):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\path\to\HCDE\tools\verify-hcde-updater.ps1
```

This script validates:

- release endpoint + asset selection
- runtime package structure
- success path (backup + replace)
- failure path (rollback restore)
- stale lock cleanup path

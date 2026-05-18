param(
    [string]$Repo = "bokoxthexchocobo/HCDE",
    [int]$StaleLockMinutes = 120
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw "ASSERT FAILED: $Message"
    }
}

function Select-HcdeReleaseAsset {
    param([object[]]$Assets)

    $zipAssets = @($Assets | Where-Object { $_.name -match '(?i)\.zip$' })
    if ($zipAssets.Count -eq 0) {
        return $null
    }

    return $zipAssets |
        Sort-Object `
            @{ Expression = {
                $name = [string]$_.name
                $score = 0
                if ($name -match '(?i)^HCDE-.*-windows-x64\.zip$') { $score += 100 }
                if ($name -match '(?i)windows.*x64|win.*x64|hcde.*x64') { $score += 40 }
                if ($name -match '(?i)^hcde') { $score += 10 }
                if ($name -match '(?i)(symbols?|pdb|debug|source|src)') { $score -= 120 }
                $score
            }; Descending = $true },
            @{ Expression = { [string]$_.name }; Descending = $false } |
        Select-Object -First 1
}

function Validate-HcdeUrl {
    param([string]$Url)

    Assert-True (-not [string]::IsNullOrWhiteSpace($Url)) "update URL is empty"
    $uri = [Uri]$Url
    Assert-True ($uri.Scheme -eq "https") "update URL must be https"

    $allowedHosts = @(
        "github.com",
        "api.github.com",
        "objects.githubusercontent.com",
        "release-assets.githubusercontent.com"
    )
    Assert-True ($allowedHosts -contains $uri.Host.ToLowerInvariant()) "update URL host is not trusted: $($uri.Host)"
}

function Get-PayloadRoot {
    param([string]$ExtractPath)

    $payloadRoot = $ExtractPath
    $children = @(Get-ChildItem -LiteralPath $ExtractPath -Force)
    if ($children.Count -eq 1 -and $children[0].PSIsContainer) {
        $payloadRoot = $children[0].FullName
    }
    return $payloadRoot
}

function Invoke-HcdeUpdaterSimulation {
    param(
        [string]$InstallDir,
        [string]$ZipPath,
        [string]$VersionTag,
        [int]$LockStaleMinutes
    )

    $resolvedInstallDir = (Resolve-Path -LiteralPath $InstallDir).ProviderPath
    $backupDir = Join-Path $resolvedInstallDir "backups"
    New-Item -ItemType Directory -Force -Path $backupDir | Out-Null

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $backupZip = Join-Path $backupDir ("hcde-backup-$VersionTag-$timestamp.zip")
    $updateLog = Join-Path $backupDir ("hcde-update-$VersionTag-$timestamp.log")
    $statusFile = Join-Path $backupDir "hcde-update-last-status.txt"
    $lockFile = Join-Path $backupDir "hcde-update.lock"
    $status = "failed"
    $statusMessage = "Updater did not complete."
    $rollbackAttempted = $false
    $rollbackSucceeded = $false
    $launchRequested = $false

    $workRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("hcde-updater-verify-" + [Guid]::NewGuid().ToString("N"))
    $extractPath = Join-Path $workRoot "extract"
    New-Item -ItemType Directory -Force -Path $workRoot | Out-Null

    if (Test-Path -LiteralPath $lockFile) {
        $existing = Get-Item -LiteralPath $lockFile -ErrorAction SilentlyContinue
        $age = if ($existing) { ((Get-Date).ToUniversalTime() - $existing.LastWriteTimeUtc).TotalMinutes } else { 0.0 }
        if ($existing -and $age -ge $LockStaleMinutes) {
            Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
            "Removed stale updater lock: $lockFile" | Out-File -LiteralPath $updateLog -Encoding utf8
        } else {
            throw "Another updater run is already in progress."
        }
    }

    @(
        "PID=$PID",
        "VERSION=$VersionTag",
        "START_UTC=$((Get-Date).ToUniversalTime().ToString('o'))"
    ) | Out-File -LiteralPath $lockFile -Encoding utf8 -Force

    try {
        $backupItems = @(Get-ChildItem -LiteralPath $resolvedInstallDir -Force | Where-Object { $_.Name -ne "backups" })
        if ($backupItems.Count -gt 0) {
            Compress-Archive -Path ($backupItems.FullName) -DestinationPath $backupZip -Force
            "Backup created: $backupZip" | Out-File -LiteralPath $updateLog -Encoding utf8
        } else {
            "No backup archive was created because the install directory had no content to archive." | Out-File -LiteralPath $updateLog -Encoding utf8
        }

        Expand-Archive -LiteralPath $ZipPath -DestinationPath $extractPath -Force
        $payloadRoot = Get-PayloadRoot -ExtractPath $extractPath
        $coreExe = Join-Path $payloadRoot "hcde.exe"
        if (-not (Test-Path -LiteralPath $coreExe)) {
            throw "Update archive did not contain hcde.exe at payload root: $payloadRoot"
        }

        Copy-Item -Path (Join-Path $payloadRoot "*") -Destination $resolvedInstallDir -Recurse -Force
        "Payload copied from: $payloadRoot" | Add-Content -LiteralPath $updateLog -Encoding utf8
        $status = "success"
        $statusMessage = "Update applied successfully."
        $launchRequested = $true
    }
    catch {
        $statusMessage = $_.Exception.Message
        if (Test-Path -LiteralPath $backupZip) {
            $rollbackAttempted = $true
            try {
                $rollbackExtractPath = Join-Path $workRoot "rollback"
                New-Item -ItemType Directory -Force -Path $rollbackExtractPath | Out-Null
                Expand-Archive -LiteralPath $backupZip -DestinationPath $rollbackExtractPath -Force
                $rollbackItems = @(Get-ChildItem -LiteralPath $rollbackExtractPath -Force)
                if ($rollbackItems.Count -gt 0) {
                    Copy-Item -Path (Join-Path $rollbackExtractPath "*") -Destination $resolvedInstallDir -Recurse -Force
                    $rollbackSucceeded = $true
                    $statusMessage = "$statusMessage (rollback restored from backup)"
                } else {
                    $statusMessage = "$statusMessage (rollback archive was empty)"
                }
            }
            catch {
                $statusMessage = "$statusMessage (rollback failed: $($_.Exception.Message))"
            }
        } else {
            $statusMessage = "$statusMessage (no rollback archive was available)"
        }
    }
    finally {
        @(
            "STATUS=$status",
            "VERSION=$VersionTag",
            "MESSAGE=$statusMessage",
            "LOG=$updateLog",
            "ROLLBACK_ATTEMPTED=$rollbackAttempted",
            "ROLLBACK_SUCCEEDED=$rollbackSucceeded",
            "TIMESTAMP_UTC=$((Get-Date).ToUniversalTime().ToString('o'))"
        ) | Out-File -LiteralPath $statusFile -Encoding utf8 -Force

        Remove-Item -LiteralPath $lockFile -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $workRoot -Recurse -Force -ErrorAction SilentlyContinue
    }

    [pscustomobject]@{
        Status = $status
        Message = $statusMessage
        LaunchRequested = $launchRequested
        RollbackAttempted = $rollbackAttempted
        RollbackSucceeded = $rollbackSucceeded
        StatusFile = $statusFile
        BackupDir = $backupDir
    }
}

function New-PackageZip {
    param(
        [string]$Root,
        [string]$PackageName,
        [bool]$IncludeExe
    )

    $stage = Join-Path $Root ("stage-" + $PackageName)
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    $payload = Join-Path $stage $PackageName
    New-Item -ItemType Directory -Force -Path $payload | Out-Null

    if ($IncludeExe) {
        "hcde placeholder" | Set-Content -LiteralPath (Join-Path $payload "hcde.exe") -Encoding ASCII
        "new marker" | Set-Content -LiteralPath (Join-Path $payload "new-marker.txt") -Encoding ASCII
    } else {
        "missing executable marker" | Set-Content -LiteralPath (Join-Path $payload "readme.txt") -Encoding ASCII
    }

    $zipPath = Join-Path $Root ($PackageName + ".zip")
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -LiteralPath $payload -DestinationPath $zipPath -CompressionLevel Optimal
    return $zipPath
}

Write-Host "== HCDE Updater Validation (Stage 3) ==" -ForegroundColor Cyan
Write-Host "Repo: $Repo"

$headers = @{ "User-Agent" = "HCDE-Updater-Verify" }
$releaseApi = "https://api.github.com/repos/$Repo/releases/latest"
$release = Invoke-RestMethod -Uri $releaseApi -Headers $headers
Assert-True (-not [string]::IsNullOrWhiteSpace([string]$release.tag_name)) "latest release tag is empty"

$asset = Select-HcdeReleaseAsset -Assets @($release.assets)
Assert-True ($null -ne $asset) "no suitable zip asset found on latest release"
Validate-HcdeUrl -Url ([string]$asset.browser_download_url)
Write-Host ("Latest: {0} | Selected asset: {1}" -f $release.tag_name, $asset.name) -ForegroundColor Green

$scratchRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("hcde-updater-verify-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratchRoot | Out-Null

try {
    $downloadZip = Join-Path $scratchRoot "downloaded-latest.zip"
    Invoke-WebRequest -Uri ([string]$asset.browser_download_url) -OutFile $downloadZip -UseBasicParsing
    $latestExtract = Join-Path $scratchRoot "latest-extract"
    Expand-Archive -LiteralPath $downloadZip -DestinationPath $latestExtract -Force
    $latestPayload = Get-PayloadRoot -ExtractPath $latestExtract
    Assert-True (Test-Path -LiteralPath (Join-Path $latestPayload "hcde.exe")) "downloaded latest asset missing hcde.exe at payload root"
    Write-Host "Latest package structure check: PASS" -ForegroundColor Green

    $install = Join-Path $scratchRoot "install"
    New-Item -ItemType Directory -Force -Path $install | Out-Null
    "old build" | Set-Content -LiteralPath (Join-Path $install "hcde.exe") -Encoding ASCII
    "old marker" | Set-Content -LiteralPath (Join-Path $install "old-marker.txt") -Encoding ASCII

    $validZip = New-PackageZip -Root $scratchRoot -PackageName "HCDE-verify-valid-windows-x64" -IncludeExe $true
    $resultSuccess = Invoke-HcdeUpdaterSimulation -InstallDir $install -ZipPath $validZip -VersionTag "v-verify-success" -LockStaleMinutes $StaleLockMinutes
    Assert-True ($resultSuccess.Status -eq "success") "success simulation did not report success"
    Assert-True (Test-Path -LiteralPath (Join-Path $install "new-marker.txt")) "success simulation did not copy payload"
    Write-Host "Updater success path (backup+replace): PASS" -ForegroundColor Green

    "old build again" | Set-Content -LiteralPath (Join-Path $install "hcde.exe") -Encoding ASCII
    "stable marker" | Set-Content -LiteralPath (Join-Path $install "stable-marker.txt") -Encoding ASCII
    $invalidZip = New-PackageZip -Root $scratchRoot -PackageName "HCDE-verify-invalid-windows-x64" -IncludeExe $false
    $resultRollback = Invoke-HcdeUpdaterSimulation -InstallDir $install -ZipPath $invalidZip -VersionTag "v-verify-failure" -LockStaleMinutes $StaleLockMinutes
    Assert-True ($resultRollback.Status -eq "failed") "failure simulation did not report failed status"
    Assert-True ($resultRollback.RollbackAttempted) "failure simulation did not attempt rollback"
    Assert-True ($resultRollback.RollbackSucceeded) "failure simulation rollback did not succeed"
    Assert-True ((Get-Content -LiteralPath (Join-Path $install "stable-marker.txt") -Raw).Contains("stable marker")) "rollback did not restore original install files"
    Write-Host "Updater failure+rollback path: PASS" -ForegroundColor Green

    $lockFile = Join-Path $resultRollback.BackupDir "hcde-update.lock"
    "stale lock" | Out-File -LiteralPath $lockFile -Encoding utf8 -Force
    (Get-Item -LiteralPath $lockFile).LastWriteTimeUtc = (Get-Date).ToUniversalTime().AddMinutes(-($StaleLockMinutes + 5))
    $resultStaleLock = Invoke-HcdeUpdaterSimulation -InstallDir $install -ZipPath $validZip -VersionTag "v-verify-stale-lock" -LockStaleMinutes $StaleLockMinutes
    Assert-True ($resultStaleLock.Status -eq "success") "stale lock simulation did not recover"
    Assert-True (-not (Test-Path -LiteralPath $lockFile)) "stale lock file was not cleaned up"
    Write-Host "Stale lock cleanup path: PASS" -ForegroundColor Green

    Write-Host ""
    Write-Host "All Stage 3 updater checks passed." -ForegroundColor Cyan
}
finally {
    Remove-Item -LiteralPath $scratchRoot -Recurse -Force -ErrorAction SilentlyContinue
}

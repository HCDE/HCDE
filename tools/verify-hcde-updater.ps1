param(
    [string]$Repo = "bokoxthexchocobo/HCDE",
    [int]$StaleLockMinutes = 120,
    [switch]$SkipLatestRelease
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem

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
    $runtimeZipAssets = @($zipAssets | Where-Object {
        $name = [string]$_.name
        (($name -match '(?i)^HCDE-.*-windows-x64\.zip$') -or ($name -match '(?i)windows.*x64|win.*x64|hcde.*x64')) -and
        ($name -notmatch '(?i)(symbols?|pdb|debug|source|src)')
    })
    if ($runtimeZipAssets.Count -eq 0) {
        return $null
    }

    return $runtimeZipAssets |
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

function Select-HcdeChecksumAsset {
    param([object[]]$Assets)

    return @($Assets | Where-Object { [string]$_.name -eq "SHA256SUMS.txt" }) |
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

function Assert-SafeZipEntries {
    param([string]$ZipPath)

    $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        Assert-True ($archive.Entries.Count -gt 0) "archive has no entries: $ZipPath"
        foreach ($entry in $archive.Entries) {
            $name = [string]$entry.FullName
            Assert-True (-not [string]::IsNullOrWhiteSpace($name)) "archive contains an empty entry name"
            Assert-True (-not $name.StartsWith("/", [System.StringComparison]::Ordinal)) "archive contains an absolute path: $name"
            Assert-True (-not $name.StartsWith("\", [System.StringComparison]::Ordinal)) "archive contains an absolute path: $name"
            Assert-True (-not ($name -match '^[A-Za-z]:')) "archive contains a drive-qualified path: $name"
            Assert-True (-not ($name -match '(^|[\\/])\.\.([\\/]|$)')) "archive contains a parent traversal path: $name"
            Assert-True (-not ($name -match '(^|[\\/])\.([\\/]|$)')) "archive contains a current-directory path segment: $name"
            Assert-True (-not ($name -match ':')) "archive contains a colon in an entry name: $name"
        }
    }
    finally {
        $archive.Dispose()
    }
}

function Expand-HcdeArchive {
    param(
        [string]$ZipPath,
        [string]$DestinationPath
    )

    Assert-SafeZipEntries -ZipPath $ZipPath
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $DestinationPath -Force
}

function Assert-HcdeRuntimePayload {
    param([string]$PayloadRoot)

    foreach ($required in @("hcde.exe", "hcdeserv.exe")) {
        Assert-True (Test-Path -LiteralPath (Join-Path $PayloadRoot $required)) "runtime payload missing required file: $required"
    }
}

function Assert-ReleaseChecksum {
    param(
        [string]$ChecksumPath,
        [string]$AssetPath,
        [string]$AssetName
    )

    $expected = $null
    foreach ($line in Get-Content -LiteralPath $ChecksumPath) {
        if ($line -match '^\s*([0-9a-fA-F]{64})\s+\*?(.+?)\s*$') {
            if ([string]$Matches[2] -eq $AssetName) {
                $expected = [string]$Matches[1]
                break
            }
        }
    }
    Assert-True (-not [string]::IsNullOrWhiteSpace($expected)) "SHA256SUMS.txt does not contain selected asset: $AssetName"

    $actual = (Get-FileHash -LiteralPath $AssetPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Assert-True ($actual -eq $expected.ToLowerInvariant()) "selected asset checksum mismatch"
}

function Wait-ForFileUnlock {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$TimeoutSeconds = 45
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $deadlineUtc = (Get-Date).ToUniversalTime().AddSeconds([Math]::Max(1, $TimeoutSeconds))
    while ($true) {
        $stream = $null
        try {
            $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
            return
        }
        catch {
            if ((Get-Date).ToUniversalTime() -ge $deadlineUtc) {
                throw "Timed out waiting for file lock release: $Path ($($_.Exception.Message))"
            }
            Start-Sleep -Milliseconds 250
        }
        finally {
            if ($stream) {
                $stream.Dispose()
            }
        }
    }
}

function Copy-TreeWithRetries {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePattern,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$Phase,
        [int]$MaxAttempts = 35
    )

    $attemptLimit = [Math]::Max(1, $MaxAttempts)
    for ($attempt = 1; $attempt -le $attemptLimit; $attempt++) {
        try {
            Copy-Item -Path $SourcePattern -Destination $Destination -Recurse -Force
            return
        }
        catch {
            if ($attempt -ge $attemptLimit) {
                throw "$Phase failed after $attempt attempts: $($_.Exception.Message)"
            }
            $delay = [Math]::Min(1500, 200 + ($attempt * 50))
            Start-Sleep -Milliseconds $delay
        }
    }
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
        foreach ($name in @("hcde.exe", "hcdeserv.exe", "hcdelaunch.exe")) {
            Wait-ForFileUnlock -Path (Join-Path $resolvedInstallDir $name) -TimeoutSeconds 45
        }

        $backupItems = @(Get-ChildItem -LiteralPath $resolvedInstallDir -Force | Where-Object { $_.Name -ne "backups" })
        if ($backupItems.Count -gt 0) {
            Compress-Archive -Path ($backupItems.FullName) -DestinationPath $backupZip -Force
            "Backup created: $backupZip" | Out-File -LiteralPath $updateLog -Encoding utf8
        } else {
            "No backup archive was created because the install directory had no content to archive." | Out-File -LiteralPath $updateLog -Encoding utf8
        }

        Expand-HcdeArchive -ZipPath $ZipPath -DestinationPath $extractPath
        $payloadRoot = Get-PayloadRoot -ExtractPath $extractPath
        Assert-HcdeRuntimePayload -PayloadRoot $payloadRoot

        Copy-TreeWithRetries -SourcePattern (Join-Path $payloadRoot "*") -Destination $resolvedInstallDir -Phase "Payload copy"
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
                Expand-HcdeArchive -ZipPath $backupZip -DestinationPath $rollbackExtractPath
                $rollbackItems = @(Get-ChildItem -LiteralPath $rollbackExtractPath -Force)
                if ($rollbackItems.Count -gt 0) {
                    Copy-TreeWithRetries -SourcePattern (Join-Path $rollbackExtractPath "*") -Destination $resolvedInstallDir -Phase "Rollback copy"
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
        "hcdeserv placeholder" | Set-Content -LiteralPath (Join-Path $payload "hcdeserv.exe") -Encoding ASCII
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

function New-UnsafePackageZip {
    param([string]$Root)

    $zipPath = Join-Path $Root "HCDE-verify-unsafe-windows-x64.zip"
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    $archive = [System.IO.Compression.ZipFile]::Open($zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $entry = $archive.CreateEntry("../evil.txt")
        $writer = [System.IO.StreamWriter]::new($entry.Open())
        try {
            $writer.Write("unsafe")
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $archive.Dispose()
    }
    return $zipPath
}

Write-Host "== HCDE Updater Validation (Stage 3) ==" -ForegroundColor Cyan
Write-Host "Repo: $Repo"

$asset = $null
$checksumAsset = $null
if ($SkipLatestRelease) {
    Write-Host "Latest release download/checksum check: SKIPPED" -ForegroundColor Yellow
} else {
    $headers = @{ "User-Agent" = "HCDE-Updater-Verify" }
    $releaseApi = "https://api.github.com/repos/$Repo/releases/latest"
    $release = Invoke-RestMethod -Uri $releaseApi -Headers $headers
    Assert-True (-not [string]::IsNullOrWhiteSpace([string]$release.tag_name)) "latest release tag is empty"

    $asset = Select-HcdeReleaseAsset -Assets @($release.assets)
    Assert-True ($null -ne $asset) "no suitable zip asset found on latest release"
    Validate-HcdeUrl -Url ([string]$asset.browser_download_url)
    $checksumAsset = Select-HcdeChecksumAsset -Assets @($release.assets)
    Assert-True ($null -ne $checksumAsset) "latest release is missing SHA256SUMS.txt"
    Validate-HcdeUrl -Url ([string]$checksumAsset.browser_download_url)
    Write-Host ("Latest: {0} | Selected asset: {1}" -f $release.tag_name, $asset.name) -ForegroundColor Green
}

$scratchRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("hcde-updater-verify-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $scratchRoot | Out-Null

try {
    if (-not $SkipLatestRelease) {
        $downloadZip = Join-Path $scratchRoot "downloaded-latest.zip"
        $downloadChecksums = Join-Path $scratchRoot "SHA256SUMS.txt"
        Invoke-WebRequest -Uri ([string]$asset.browser_download_url) -OutFile $downloadZip -UseBasicParsing
        Invoke-WebRequest -Uri ([string]$checksumAsset.browser_download_url) -OutFile $downloadChecksums -UseBasicParsing
        Assert-ReleaseChecksum -ChecksumPath $downloadChecksums -AssetPath $downloadZip -AssetName ([string]$asset.name)
        Write-Host "Latest package checksum check: PASS" -ForegroundColor Green

        $latestExtract = Join-Path $scratchRoot "latest-extract"
        Expand-HcdeArchive -ZipPath $downloadZip -DestinationPath $latestExtract
        $latestPayload = Get-PayloadRoot -ExtractPath $latestExtract
        Assert-HcdeRuntimePayload -PayloadRoot $latestPayload
        Write-Host "Latest package structure check: PASS" -ForegroundColor Green
    }

    $install = Join-Path $scratchRoot "install"
    New-Item -ItemType Directory -Force -Path $install | Out-Null
    "old build" | Set-Content -LiteralPath (Join-Path $install "hcde.exe") -Encoding ASCII
    "old marker" | Set-Content -LiteralPath (Join-Path $install "old-marker.txt") -Encoding ASCII

    $validZip = New-PackageZip -Root $scratchRoot -PackageName "HCDE-verify-valid-windows-x64" -IncludeExe $true
    $resultSuccess = Invoke-HcdeUpdaterSimulation -InstallDir $install -ZipPath $validZip -VersionTag "v-verify-success" -LockStaleMinutes $StaleLockMinutes
    Assert-True ($resultSuccess.Status -eq "success") "success simulation did not report success"
    Assert-True (Test-Path -LiteralPath (Join-Path $install "new-marker.txt")) "success simulation did not copy payload"
    Write-Host "Updater success path (backup+replace): PASS" -ForegroundColor Green

    "old build lock retry" | Set-Content -LiteralPath (Join-Path $install "hcde.exe") -Encoding ASCII
    $lockJob = Start-Job -ScriptBlock {
        param([string]$Path)
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        try {
            Start-Sleep -Seconds 3
        }
        finally {
            $stream.Dispose()
        }
    } -ArgumentList (Join-Path $install "hcde.exe")
    try {
        $resultLockRetry = Invoke-HcdeUpdaterSimulation -InstallDir $install -ZipPath $validZip -VersionTag "v-verify-lock-retry" -LockStaleMinutes $StaleLockMinutes
        Assert-True ($resultLockRetry.Status -eq "success") "transient file-lock simulation did not recover"
        Write-Host "Updater transient file-lock retry path: PASS" -ForegroundColor Green
    }
    finally {
        Wait-Job -Job $lockJob -ErrorAction SilentlyContinue | Out-Null
        Receive-Job -Job $lockJob -ErrorAction SilentlyContinue | Out-Null
        Remove-Job -Job $lockJob -Force -ErrorAction SilentlyContinue
    }

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

    $unsafeZip = New-UnsafePackageZip -Root $scratchRoot
    $unsafeRejected = $false
    try {
        Assert-SafeZipEntries -ZipPath $unsafeZip
    }
    catch {
        $unsafeRejected = $true
    }
    Assert-True $unsafeRejected "archive path traversal fixture was not rejected"
    Write-Host "Archive path traversal rejection: PASS" -ForegroundColor Green

    Write-Host ""
    Write-Host "All Stage 3 updater checks passed." -ForegroundColor Cyan
}
finally {
    Remove-Item -LiteralPath $scratchRoot -Recurse -Force -ErrorAction SilentlyContinue
}

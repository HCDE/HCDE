param(
    [string]$Version = "0.4.4",
    [string]$Configuration = "RelWithDebInfo",
    [string]$OpenALSoftVersion = "1.25.2",
    [string]$SndFileDll = "",
    [string]$SndFileLicense = "",
    [switch]$Build,
    [switch]$IncludeSymbols,
    [switch]$Upload,
    [string]$Repo = "HCDE/HCDE",
    [string]$ReleaseTag = "",
    [string]$GhCli = "gh"
)

$ErrorActionPreference = "Stop"

# Local-first release policy: this script always creates Windows x64 release
# packages under build\releases and writes SHA-256 checksums there. GitHub is
# only used when -Upload is explicitly supplied.

function Resolve-RequiredPath {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required path not found: $Path"
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Assert-ChildPath {
    param(
        [string]$Parent,
        [string]$Child
    )

    $resolvedParent = (Resolve-Path -LiteralPath $Parent).Path
    if (Test-Path -LiteralPath $Child) {
        $resolvedChild = (Resolve-Path -LiteralPath $Child).Path
    }
    else {
        $resolvedChild = [System.IO.Path]::GetFullPath($Child)
    }

    if (-not $resolvedChild.StartsWith($resolvedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to operate outside $resolvedParent`: $resolvedChild"
    }

    return $resolvedChild
}

function Write-ReleaseChecksums {
    param(
        [Parameter(Mandatory)][string]$OutputPath,
        [Parameter(Mandatory)][string[]]$Assets
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($asset in $Assets) {
        if (-not (Test-Path -LiteralPath $asset)) {
            throw "Missing checksum asset: $asset"
        }
        $hash = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("$hash  $([System.IO.Path]::GetFileName($asset))")
    }
    Set-Content -LiteralPath $OutputPath -Encoding ASCII -Value $lines
    Write-Host "Checksums: $OutputPath"
}

function Resolve-OpenALSoftRuntime {
    param(
        [string]$BuildRoot,
        [string]$Version
    )

    $depsDir = Join-Path $BuildRoot "deps"
    if (-not (Test-Path -LiteralPath $depsDir)) {
        New-Item -Path $depsDir -ItemType Directory | Out-Null
    }
    $depsDir = Resolve-RequiredPath $depsDir

    $archiveName = "openal-soft-$Version-bin.zip"
    $archivePath = Join-Path $depsDir $archiveName
    $extractDir = Join-Path $depsDir "openal-soft-$Version-bin"
    $archiveUrl = "https://openal-soft.org/openal-binaries/$archiveName"

    if (-not (Test-Path -LiteralPath $archivePath)) {
        Write-Host "Downloading OpenAL Soft $Version..."
        Invoke-WebRequest -Uri $archiveUrl -OutFile $archivePath
    }

    if (-not (Test-Path -LiteralPath $extractDir)) {
        New-Item -Path $extractDir -ItemType Directory | Out-Null
        Expand-Archive -LiteralPath $archivePath -DestinationPath $extractDir -Force
    }

    $payloadRoot = Join-Path $extractDir "openal-soft-$Version-bin"
    [pscustomobject]@{
        Dll = Resolve-RequiredPath (Join-Path $payloadRoot "bin/Win64/soft_oal.dll")
        License = Resolve-RequiredPath (Join-Path $payloadRoot "COPYING")
    }
}

function Resolve-SndFileRuntime {
    param(
        [string]$BuildRoot,
        [string]$BuildConfigDir,
        [string]$ExplicitDll,
        [string]$ExplicitLicense
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitDll)) {
        $candidates += $ExplicitDll
    }
    $candidates += (Join-Path $BuildConfigDir "sndfile.dll")
    $candidates += (Join-Path $BuildRoot "deps/sndfile.dll")

    $userProfile = [Environment]::GetFolderPath("UserProfile")
    if (-not [string]::IsNullOrWhiteSpace($userProfile)) {
        $candidates += (Join-Path $userProfile "Downloads/gzdoom-4-14-2-windows/sndfile.dll")
    }

    $missingLicenseDll = $null
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate) -or -not (Test-Path -LiteralPath $candidate)) {
            continue
        }

        $dll = Resolve-RequiredPath $candidate
        $licenseCandidates = @()
        if (-not [string]::IsNullOrWhiteSpace($ExplicitLicense)) {
            $licenseCandidates += $ExplicitLicense
        }
        $dllDir = Split-Path -Parent $dll
        $licenseCandidates += (Join-Path $dllDir "lgpl.txt")
        $licenseCandidates += (Join-Path $dllDir "COPYING.LIB")
        $licenseCandidates += (Join-Path $dllDir "COPYING.LESSER")

        $license = $licenseCandidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_) } | Select-Object -First 1
        $licensesZip = Join-Path $dllDir "licenses.zip"
        if (-not $license -and (Test-Path -LiteralPath $licensesZip)) {
            $licenseDir = Join-Path $BuildRoot "deps/sndfile-license"
            if (-not (Test-Path -LiteralPath $licenseDir)) {
                New-Item -Path $licenseDir -ItemType Directory | Out-Null
            }
            Expand-Archive -LiteralPath $licensesZip -DestinationPath $licenseDir -Force
            $license = Get-ChildItem -LiteralPath $licenseDir -Recurse -File |
                Where-Object { $_.Name -match '^(lgpl|copying)' } |
                Select-Object -ExpandProperty FullName -First 1
        }

        if ($license) {
            return [pscustomobject]@{
                Dll = $dll
                License = Resolve-RequiredPath $license
            }
        }

        $missingLicenseDll = $dll
    }

    if ($missingLicenseDll) {
        throw "Found sndfile.dll at $missingLicenseDll, but no LGPL/license text beside any candidate. Pass -SndFileLicense to keep the release package complete."
    }

    throw "Required sndfile.dll not found. HCDE uses it for WAV/OGG mod sounds; pass -SndFileDll or place sndfile.dll in build\\deps."
}

function Get-ModCompatRuntimeFiles {
    param(
        [string]$BuildRoot,
        [string]$BuildConfigDir
    )

    $requiredCompatNames = @(
        "hcde_mod_compat_combined.pk3",
        "hcde_mod_compat_pink_valley_eng.pk3"
    )

    $compatFiles = @()
    foreach ($compatName in $requiredCompatNames) {
        $candidatePaths = @(
            (Join-Path $BuildConfigDir $compatName),
            (Join-Path $BuildRoot $compatName)
        )

        $resolved = $candidatePaths |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1

        if (-not $resolved) {
            throw "Required HCDE mod compatibility PK3 '$compatName' was not found. Build the compat targets before packaging."
        }

        $compatFiles += Get-Item -LiteralPath $resolved
    }

    return $compatFiles
}

function Upload-ReleaseAssets {
    param(
        [string]$GhCliPath,
        [string]$Repository,
        [string]$Tag,
        [string]$VersionLabel,
        [string]$ReleaseNotesFile,
        [string[]]$Assets
    )

    $gh = $GhCliPath
    $resolvedGh = Get-Command $gh -ErrorAction SilentlyContinue
    if ($null -eq $resolvedGh) {
        throw "GitHub CLI not found: $gh"
    }
    $gh = $resolvedGh.Source

    $releaseExists = $false
    $prevEa = $ErrorActionPreference
    try {
        # gh returns non-zero and writes to stderr when a release is missing.
        # Treat that as "not found" instead of a terminating PowerShell error.
        $ErrorActionPreference = "Continue"
        & $gh release view $Tag --repo $Repository 1>$null 2>$null
        if ($LASTEXITCODE -eq 0) {
            $releaseExists = $true
        }
    }
    finally {
        $ErrorActionPreference = $prevEa
    }

    if (-not $releaseExists) {
        & $gh release create $Tag `
            --repo $Repository `
            --title "HCDE v$VersionLabel" `
            --notes-file $ReleaseNotesFile
        if ($LASTEXITCODE -ne 0) {
            throw "gh release create failed."
        }
    }
    else {
        & $gh release edit $Tag `
            --repo $Repository `
            --title "HCDE v$VersionLabel" `
            --notes-file $ReleaseNotesFile `
            --draft=false
        if ($LASTEXITCODE -ne 0) {
            throw "gh release edit failed."
        }
    }

    & $gh release upload $Tag @Assets --repo $Repository --clobber
    if ($LASTEXITCODE -ne 0) {
        throw "gh release upload failed."
    }

    $remoteAssets = & $gh release view $Tag --repo $Repository --json assets --jq '.assets[].name'
    if ($LASTEXITCODE -ne 0) {
        throw "gh release asset verification failed."
    }
    foreach ($asset in $Assets) {
        $assetName = [System.IO.Path]::GetFileName($asset)
        if (-not ($remoteAssets -contains $assetName)) {
            throw "Release asset verification failed. Missing remote asset: $assetName"
        }
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$buildRoot = Join-Path $repoRoot "build"
$buildConfigDir = Join-Path $buildRoot $Configuration
$releaseRoot = Join-Path $buildRoot "releases"
$packageName = "HCDE-$Version-windows-x64"
$compatPackageName = "HCDE-$Version-compat-mods"
$stageDir = Join-Path $releaseRoot $packageName
$compatStageDir = Join-Path $releaseRoot $compatPackageName
$packageZip = Join-Path $releaseRoot "$packageName.zip"
$compatZip = Join-Path $releaseRoot "$compatPackageName.zip"
$symbolsZip = Join-Path $releaseRoot "$packageName-symbols.zip"

if ($Build) {
    cmake --build $buildRoot --config $Configuration --target zdoom --parallel 1
    cmake --build $buildRoot --config $Configuration --target hcdeserv --parallel 1
}

$buildConfigDir = Resolve-RequiredPath $buildConfigDir
if (-not (Test-Path -LiteralPath $releaseRoot)) {
    New-Item -Path $releaseRoot -ItemType Directory | Out-Null
}
$releaseRoot = Resolve-RequiredPath $releaseRoot
$stageDir = Assert-ChildPath -Parent $releaseRoot -Child $stageDir
$compatStageDir = Assert-ChildPath -Parent $releaseRoot -Child $compatStageDir
$packageZip = Assert-ChildPath -Parent $releaseRoot -Child $packageZip
$compatZip = Assert-ChildPath -Parent $releaseRoot -Child $compatZip
$symbolsZip = Assert-ChildPath -Parent $releaseRoot -Child $symbolsZip

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -Path $stageDir -ItemType Directory | Out-Null

if (Test-Path -LiteralPath $compatStageDir) {
    Remove-Item -LiteralPath $compatStageDir -Recurse -Force
}
New-Item -Path $compatStageDir -ItemType Directory | Out-Null

$compatFiles = Get-ModCompatRuntimeFiles -BuildRoot $buildRoot -BuildConfigDir $buildConfigDir

$runtimeFiles = @(
    "hcde.exe",
    "hcdeserv.exe",
    "hcde.pk3",
    "game_support.pk3",
    "brightmaps.pk3",
    "lights.pk3",
    "game_widescreen_gfx.pk3"
)

foreach ($file in $runtimeFiles) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir $file)) -Destination $stageDir
}

$runtimeDirs = @("fm_banks")
foreach ($dir in $runtimeDirs) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir $dir)) -Destination $stageDir -Recurse
}

$soundfontsDir = Join-Path $stageDir "soundfonts"
New-Item -Path $soundfontsDir -ItemType Directory | Out-Null
Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir "soundfonts/hcde.sf2")) -Destination $soundfontsDir

$docFiles = @("README.md", "LICENSE")
foreach ($file in $docFiles) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $repoRoot $file)) -Destination $stageDir
}

$openALSoft = Resolve-OpenALSoftRuntime -BuildRoot $buildRoot -Version $OpenALSoftVersion
Copy-Item -LiteralPath $openALSoft.Dll -Destination $stageDir
Copy-Item -LiteralPath $openALSoft.License -Destination (Join-Path $stageDir "OPENAL-SOFT-COPYING.txt")

$sndFile = Resolve-SndFileRuntime -BuildRoot $buildRoot -BuildConfigDir $buildConfigDir -ExplicitDll $SndFileDll -ExplicitLicense $SndFileLicense
Copy-Item -LiteralPath $sndFile.Dll -Destination (Join-Path $stageDir "sndfile.dll")
Copy-Item -LiteralPath $sndFile.License -Destination (Join-Path $stageDir "SNDFILE-LICENSE.txt")

@"
HCDE $Version

This package contains the Windows x64 HCDE client and dedicated server.

Included:
- hcde.exe
- hcdeserv.exe
- HCDE runtime PK3 files
- FM bank and soundfont assets
- OpenAL Soft runtime for client audio
- libsndfile runtime for WAV/OGG mod sounds

Bring your own IWAD, such as DOOM2.WAD.

Source code and complete documentation are available from the public HCDE repository:
https://github.com/HCDE/HCDE

The corresponding source for this release is the v$Version tag:
https://github.com/HCDE/HCDE/tree/v$Version
"@ | Set-Content -LiteralPath (Join-Path $stageDir "RELEASE-NOTES.txt") -Encoding UTF8

foreach ($compatFile in $compatFiles) {
    Copy-Item -LiteralPath $compatFile.FullName -Destination $compatStageDir -Force
}

@"
HCDE $Version compat add-ons

This package contains optional HCDE-owned compatibility resources.
These files are not part of the core engine runtime package and can be loaded
as extras alongside mods they target.

Included:
- hcde_mod_compat_combined.pk3
- hcde_mod_compat_pink_valley_eng.pk3
"@ | Set-Content -LiteralPath (Join-Path $compatStageDir "README.txt") -Encoding UTF8

if (Test-Path -LiteralPath $packageZip) {
    Remove-Item -LiteralPath $packageZip -Force
}
Compress-Archive -LiteralPath $stageDir -DestinationPath $packageZip -CompressionLevel Optimal

if (Test-Path -LiteralPath $compatZip) {
    Remove-Item -LiteralPath $compatZip -Force
}
Compress-Archive -LiteralPath $compatStageDir -DestinationPath $compatZip -CompressionLevel Optimal

if ($IncludeSymbols) {
    $symbolsStage = Join-Path $releaseRoot "$packageName-symbols"
    $symbolsStage = Assert-ChildPath -Parent $releaseRoot -Child $symbolsStage
    if (Test-Path -LiteralPath $symbolsStage) {
        Remove-Item -LiteralPath $symbolsStage -Recurse -Force
    }
    New-Item -Path $symbolsStage -ItemType Directory | Out-Null
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir "hcde.pdb")) -Destination $symbolsStage
    if (Test-Path -LiteralPath $symbolsZip) {
        Remove-Item -LiteralPath $symbolsZip -Force
    }
    Compress-Archive -LiteralPath $symbolsStage -DestinationPath $symbolsZip -CompressionLevel Optimal
}

Write-Host "Runtime package: $packageZip"
Write-Host "Compat package: $compatZip"
if ($IncludeSymbols) {
    Write-Host "Symbols package: $symbolsZip"
}

$checksumAssets = [System.Collections.Generic.List[string]]::new()
$checksumAssets.Add($packageZip)
$checksumAssets.Add($compatZip)
if ($IncludeSymbols) {
    $checksumAssets.Add($symbolsZip)
}
Write-ReleaseChecksums -OutputPath (Join-Path $releaseRoot "SHA256SUMS.txt") -Assets $checksumAssets.ToArray()

if ($Upload) {
    $resolvedTag = $ReleaseTag
    if ([string]::IsNullOrWhiteSpace($resolvedTag)) {
        $resolvedTag = "v$Version"
    }

    $releaseNotesFile = Join-Path $releaseRoot "HCDE-$Version-release-notes.txt"
    @"
HCDE $Version

- Windows x64 runtime package uploaded as a standalone asset.
- Optional HCDE compatibility PK3 files uploaded as a separate compat zip.
"@ | Set-Content -LiteralPath $releaseNotesFile -Encoding UTF8

    $assets = [System.Collections.Generic.List[string]]::new()
    $assets.Add($packageZip)
    $assets.Add($compatZip)
    if ($IncludeSymbols) {
        $assets.Add($symbolsZip)
    }

    Upload-ReleaseAssets `
        -GhCliPath $GhCli `
        -Repository $Repo `
        -Tag $resolvedTag `
        -VersionLabel $Version `
        -ReleaseNotesFile $releaseNotesFile `
        -Assets $assets.ToArray()

    Write-Host "Uploaded release assets to $Repo tag $resolvedTag"
}

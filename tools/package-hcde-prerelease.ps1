param(
    [string]$Version = "0.4.2-hotfix1",
    [string]$Configuration = "RelWithDebInfo",
    [string]$OpenALSoftVersion = "1.25.2",
    [string]$SndFileDll = "",
    [string]$SndFileLicense = "",
    [switch]$Build,
    [switch]$IncludeSymbols
)

$ErrorActionPreference = "Stop"

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

function Copy-ModCompatRuntime {
    param(
        [string]$BuildRoot,
        [string]$BuildConfigDir,
        [string]$StageDir
    )

    $compatFiles = @()
    $compatFiles += Get-ChildItem -LiteralPath $BuildConfigDir -Filter "hcde_mod_compat*.pk3" -File -ErrorAction SilentlyContinue
    $compatFiles += Get-ChildItem -LiteralPath $BuildRoot -Filter "hcde_mod_compat*.pk3" -File -ErrorAction SilentlyContinue
    $compatFiles = $compatFiles |
        Group-Object Name |
        ForEach-Object { $_.Group | Sort-Object FullName -Descending | Select-Object -First 1 }

    if (-not $compatFiles -or $compatFiles.Count -eq 0) {
        throw "Required HCDE mod compatibility PK3 files were not found. Build the compat targets before packaging."
    }

    foreach ($file in $compatFiles) {
        Copy-Item -LiteralPath $file.FullName -Destination $StageDir
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$buildRoot = Join-Path $repoRoot "build"
$buildConfigDir = Join-Path $buildRoot $Configuration
$releaseRoot = Join-Path $buildRoot "releases"
$packageName = "HCDE-$Version-windows-x64"
$stageDir = Join-Path $releaseRoot $packageName
$packageZip = Join-Path $releaseRoot "$packageName.zip"
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
$packageZip = Assert-ChildPath -Parent $releaseRoot -Child $packageZip
$symbolsZip = Assert-ChildPath -Parent $releaseRoot -Child $symbolsZip

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -Path $stageDir -ItemType Directory | Out-Null

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
Copy-ModCompatRuntime -BuildRoot $buildRoot -BuildConfigDir $buildConfigDir -StageDir $stageDir

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
https://github.com/bokoxthexchocobo/HCDE

The corresponding source for this release is the v$Version tag:
https://github.com/bokoxthexchocobo/HCDE/tree/v$Version
"@ | Set-Content -LiteralPath (Join-Path $stageDir "RELEASE-NOTES.txt") -Encoding UTF8

if (Test-Path -LiteralPath $packageZip) {
    Remove-Item -LiteralPath $packageZip -Force
}
Compress-Archive -LiteralPath $stageDir -DestinationPath $packageZip -CompressionLevel Optimal

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
if ($IncludeSymbols) {
    Write-Host "Symbols package: $symbolsZip"
}

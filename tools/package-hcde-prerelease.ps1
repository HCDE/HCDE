param(
    [string]$Version = "0.1.0",
    [string]$Configuration = "RelWithDebInfo",
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

$runtimeDirs = @("fm_banks")
foreach ($dir in $runtimeDirs) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir $dir)) -Destination $stageDir -Recurse
}

$soundfontsDir = Join-Path $stageDir "soundfonts"
New-Item -Path $soundfontsDir -ItemType Directory | Out-Null
Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $buildConfigDir "soundfonts/hcde.sf2")) -Destination $soundfontsDir

$docFiles = @("README.md", "LICENSE", "SECURITY.md", "docs/HCDE_GOLDEN_RULES.md")
foreach ($file in $docFiles) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $repoRoot $file)) -Destination $stageDir
}

@"
HCDE $Version prerelease

This package contains the Windows x64 HCDE client and dedicated server.

Included:
- hcde.exe
- hcdeserv.exe
- HCDE runtime PK3 files
- FM bank and soundfont assets

Bring your own IWAD, such as DOOM2.WAD.
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

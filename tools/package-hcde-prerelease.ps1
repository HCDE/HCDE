param(
    [string]$Version = "0.2.0",
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
    cmake --build $buildRoot --config $Configuration --target hcdeserv --parallel 1
    cmake --build $buildRoot --config $Configuration --target hcdemaster --parallel 1
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
    "hcdemaster.exe",
    "hcde.pk3",
    "hcde_mod_compat.pk3",
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

$docFiles = @("README.md", "LICENSE", "SECURITY.md", "docs/HCDE_GOLDEN_RULES.md", "docs/HCDE_MASTER_PROTOCOL.md", "docs/HCDE_MASTER_BOUNDARY_STAGE1_AUDIT.md", "docs/HCDE_NETCODE_STAGE15_SERVER_SNAPSHOT_PAYLOAD.md", "docs/HCDE_NETCODE_STAGE16_CLIENT_INPUT_RECORDS.md", "docs/HCDE_NETCODE_STAGE17_SERVER_SNAPSHOT_RECORDS.md", "docs/HCDE_NETCODE_STAGE18_CLIENT_INPUT_COMMAND_FIELDS.md", "docs/HCDE_NETCODE_STAGE19_CLIENT_INPUT_EVENT_RECORDS.md", "docs/HCDE_NETCODE_STAGE20_CLIENT_INPUT_EVENT_PAYLOADS.md", "docs/HCDE_NETCODE_STAGE21_SERVER_SNAPSHOT_COMMAND_RECORDS.md", "docs/HCDE_NETCODE_STAGE22_DEDICATED_INPUT_AUTHORITY.md", "docs/HCDE_NETCODE_STAGE23_SERVER_WORLD_DELTAS.md", "docs/HCDE_NETCODE_STAGE24_BASELINE_REPAIR.md", "docs/HCDE_NETCODE_STAGE25_CLIENT_RECONCILIATION.md", "docs/HCDE_NETCODE_STAGE26_SLOTLESS_DEDICATED_BOUNDARY.md", "docs/HCDE_NETCODE_STAGE27_LIVE_ROUTE_AUTHORITY.md", "docs/HCDE_NETCODE_STAGE28_NON_PLAYER_SERVER_SESSION.md", "docs/HCDE_NETCODE_STAGE29_SERVICE_AUTHORITY_IDENTITY.md", "docs/HCDE_NETCODE_STAGE30_PACKET_AUTHORITY_BOUNDARY.md", "docs/HCDE_NETCODE_STAGE31_PACKET_BUILD_AUTHORITY.md", "docs/HCDE_NETCODE_STAGE32_QUIT_AUTHORITY.md", "docs/HCDE_NETCODE_STAGE33_STATUS_AUTHORITY.md")
foreach ($file in $docFiles) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $repoRoot $file)) -Destination $stageDir
}

$protocolDir = Join-Path $stageDir "protocol"
New-Item -Path $protocolDir -ItemType Directory | Out-Null
$protocolFiles = @("protocol/hcde_master_protocol.json", "protocol/hcde_master_protocol.h")
foreach ($file in $protocolFiles) {
    Copy-Item -LiteralPath (Resolve-RequiredPath (Join-Path $repoRoot $file)) -Destination $protocolDir
}

@"
HCDE $Version

This package contains the Windows x64 HCDE client, dedicated server, and standalone master server.

Included:
- hcde.exe
- hcdeserv.exe
- hcdemaster.exe
- HCDE runtime PK3 files
- FM bank and soundfont assets

Bring your own IWAD, such as DOOM2.WAD.

Master discovery is opt-in for dedicated servers. Start hcdemaster.exe, then launch hcdeserv.exe
with +set sv_usemasters 1 -master host[:port], or configure Doom Connector's HCDE masterserver
setting so Connector appends those startup arguments for hosted sessions.
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

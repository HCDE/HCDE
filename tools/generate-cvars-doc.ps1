param(
    [string]$BaselineCsv = "C:\Users\user\DoomConnector6\tmp_cvar_baseline.csv",
    [string]$SetpassReport = "C:\Users\user\DoomConnector6\tmp_cvar_setpass_report.json",
    [string]$OutputPath = "",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $PSScriptRoot "..\docs\Cvars.md"
}
if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).ProviderPath
}

$BaselineCsv = (Resolve-Path -LiteralPath $BaselineCsv).ProviderPath
$SetpassReport = (Resolve-Path -LiteralPath $SetpassReport).ProviderPath

$outDir = Split-Path -Parent $OutputPath
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$cvars = Import-Csv -LiteralPath $BaselineCsv | Sort-Object Name
$report = Get-Content -LiteralPath $SetpassReport -Raw | ConvertFrom-Json

$latchedSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach($n in @($report.latchedMessages)) {
    [void]$latchedSet.Add([string]$n)
}

$missingGetSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach($n in @($report.missingGetResponses)) {
    [void]$missingGetSet.Add([string]$n)
}

$writeProtectedSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach($n in @($report.writeProtectedResponses)) {
    [void]$writeProtectedSet.Add([string]$n)
}

$runtimeNameSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($c in $cvars) {
    [void]$runtimeNameSet.Add([string]$c.Name)
}

function Decode-Flags {
    param([string]$Flags)

    if ($null -eq $Flags) {
        $Flags = ""
    }

    $chars = $Flags.PadRight(5).ToCharArray()

    $archive = if ($chars[0] -eq 'A') { 'Yes' } else { 'No' }
    $scope = switch ($chars[1]) {
        'U' { 'UserInfo (replicated per user)' }
        'S' { 'ServerInfo (server-advertised)' }
        'C' { 'Auto/Custom (runtime-created)' }
        default { 'Local/General' }
    }
    $mutability = switch ($chars[2]) {
        '-' { 'Write-protected (NOSET)' }
        'L' { 'Latched (applies next game/level)' }
        '*' { 'Unsettable auto cvar' }
        default { 'Writable now' }
    }
    $isMod = if ($chars[3] -eq 'M') { 'Yes' } else { 'No' }
    $isIgnored = if ($chars[4] -eq 'X') { 'Yes' } else { 'No' }

    return [pscustomobject]@{
        Archive = $archive
        Scope = $scope
        Mutability = $mutability
        Mod = $isMod
        Ignored = $isIgnored
    }
}

function Get-ReadableHintFromName {
    param([string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return "No description available."
    }

    $work = $Name
    $scope = ""
    if ($work.StartsWith("sv_")) {
        $scope = "server"
        $work = $work.Substring(3)
    } elseif ($work.StartsWith("cl_")) {
        $scope = "client"
        $work = $work.Substring(3)
    } elseif ($work.StartsWith("co_")) {
        $scope = "co-op"
        $work = $work.Substring(3)
    } elseif ($work.StartsWith("g_")) {
        $scope = "gameplay"
        $work = $work.Substring(2)
    } elseif ($work.StartsWith("net_")) {
        $scope = "network"
        $work = $work.Substring(4)
    }

    $tokens = $work -split "_"
    $tokenText = (($tokens | Where-Object { $_ -ne "" }) -join " ").Trim()
    if ([string]::IsNullOrWhiteSpace($tokenText)) {
        $tokenText = $Name
    }

    if ([string]::IsNullOrWhiteSpace($scope)) {
        return ("Likely controls {0}." -f $tokenText)
    }
    return ("Likely controls {0} behavior for {1}." -f $tokenText, $scope)
}

function Get-SourceDescriptions {
    param([string]$Root)

    $map = @{}
    $srcRoot = Join-Path $Root "src"
    if (-not (Test-Path -LiteralPath $srcRoot)) {
        return $map
    }

    $files = Get-ChildItem -Path $srcRoot -Recurse -Include *.cpp,*.h -File
    $rxCvarde = [regex]'(?:CUSTOM_CVARD|CVARD)\(\s*[^,]+,\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*[^,]+,\s*[^,]+,\s*([^)]+)\)'

    foreach ($file in $files) {
        $content = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($m in $rxCvarde.Matches($content)) {
            $name = [string]$m.Groups[1].Value
            $descRaw = [string]$m.Groups[2].Value
            if ([string]::IsNullOrWhiteSpace($name) -or [string]::IsNullOrWhiteSpace($descRaw)) {
                continue
            }
            $desc = $descRaw.Trim().Trim('"')
            if (-not [string]::IsNullOrWhiteSpace($desc)) {
                $map[$name] = $desc
            }
        }
    }
    return $map
}

function Get-ServerGuiDescriptions {
    param([string]$Root)

    $map = @{}
    $file = Join-Path $Root "src\common\platform\win32\i_mainwindow.cpp"
    if (-not (Test-Path -LiteralPath $file)) {
        return $map
    }

    $lines = Get-Content -LiteralPath $file
    $rx = [regex]'\{\s*L"([^"]+)"\s*,\s*"([A-Za-z_][A-Za-z0-9_]*)"'
    foreach ($line in $lines) {
        $m = $rx.Match($line)
        if (-not $m.Success) {
            continue
        }
        $label = [string]$m.Groups[1].Value
        $name = [string]$m.Groups[2].Value
        if ([string]::IsNullOrWhiteSpace($label) -or [string]::IsNullOrWhiteSpace($name)) {
            continue
        }
        $map[$name] = ("Server setting: {0}" -f $label)
    }
    return $map
}

$manualDescriptions = @{
    "sv_invasioncountdowntime"    = 'Seconds before wave 1 starts ("Prepare for invasion" countdown).'
    "sv_invasionspawntime"        = 'Wave spawn window length in seconds before cleanup phase.'
    "sv_invasioncleanuptime"      = 'Seconds allowed for cleanup phase after spawning ends.'
    "sv_invasionintermissiontime" = 'Seconds between completed waves before the next wave starts.'
    "sv_invasionresulttime"       = 'Seconds to keep the final victory/failure state visible.'
    "sv_invasionwaves"            = 'Maximum number of invasion waves in a run.'
    "sv_invasionbasebudget"       = 'Base monster budget each wave starts with.'
    "sv_invasionbudgetstep"       = 'Budget increase applied per wave number.'
    "sv_invasionperplayer"        = 'Additional budget per extra active player.'
    "sv_invasionspawninterval"    = 'Seconds between spawn ticks while wave spawning is active.'
    "sv_invasionspawnburst"       = 'Maximum monsters spawned per spawn tick burst.'
    "sv_invasionbosswaveevery"    = 'Boss wave cadence (e.g. 5 = every 5th wave, 0 = never).'
    "sv_invasionbossbonus"        = 'Extra budget added during boss waves.'
    "sv_invasionspotusemaptags"   = 'Restrict native invasion spots by map thing TID/tag. Keep disabled for Skulltag/Zandronum map compatibility; the spot arguments already control wave timing.'
    "sv_invasionspotfallback"     = 'Fallback to generic spawning when tagged invasion spots cannot be used.'
}

$invasionRanges = @{
    "sv_invasioncountdowntime"    = ">= 0"
    "sv_invasionspawntime"        = ">= 0"
    "sv_invasioncleanuptime"      = ">= 0"
    "sv_invasionintermissiontime" = ">= 0"
    "sv_invasionresulttime"       = ">= 0"
    "sv_invasionwaves"            = "1..255"
    "sv_invasionbasebudget"       = ">= 1"
    "sv_invasionbudgetstep"       = ">= 0"
    "sv_invasionperplayer"        = ">= 0"
    "sv_invasionspawninterval"    = ">= 0.05"
    "sv_invasionspawnburst"       = ">= 1"
    "sv_invasionbosswaveevery"    = ">= 0"
    "sv_invasionbossbonus"        = ">= 0"
    "sv_invasionspotusemaptags"   = "bool"
    "sv_invasionspotfallback"     = "bool"
}

$sourceDescriptions = Get-SourceDescriptions -Root $RepoRoot
$serverGuiDescriptions = Get-ServerGuiDescriptions -Root $RepoRoot

function Get-DescriptionForCvar {
    param([string]$Name)

    if ($manualDescriptions.ContainsKey($Name)) {
        return [string]$manualDescriptions[$Name]
    }
    if ($sourceDescriptions.ContainsKey($Name)) {
        return [string]$sourceDescriptions[$Name]
    }
    if ($serverGuiDescriptions.ContainsKey($Name)) {
        return [string]$serverGuiDescriptions[$Name]
    }
    return (Get-ReadableHintFromName -Name $Name)
}

$invasionDefaultFallback = @{
    "sv_invasioncountdowntime"    = "30"
    "sv_invasionspawntime"        = "8"
    "sv_invasioncleanuptime"      = "4"
    "sv_invasionintermissiontime" = "6"
    "sv_invasionresulttime"       = "8"
    "sv_invasionwaves"            = "8"
    "sv_invasionbasebudget"       = "24"
    "sv_invasionbudgetstep"       = "8"
    "sv_invasionperplayer"        = "6"
    "sv_invasionspawninterval"    = "0.35"
    "sv_invasionspawnburst"       = "3"
    "sv_invasionbosswaveevery"    = "5"
    "sv_invasionbossbonus"        = "20"
    "sv_invasionspotusemaptags"   = "0"
    "sv_invasionspotfallback"     = "1"
}

$stampUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss")

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# HCDE CVAR Reference")
$lines.Add("")
$lines.Add(("Generated: {0} UTC" -f $stampUtc))
$lines.Add("")
$lines.Add('This reference combines live runtime CVAR output and source-level metadata.')
$lines.Add("")
$lines.Add("## Coverage")
$lines.Add("")
$lines.Add(('- Total runtime CVARs discovered: **{0}**' -f $report.parsedCvars))
$lines.Add(('- Set/get tested runtime CVARs: **{0}**' -f $report.setTargets))
$lines.Add(('- Successful get responses: **{0}**' -f $report.getResponses))
$lines.Add(('- Missing get responses: **{0}**' -f @($report.missingGetResponses).Count))
$lines.Add(('- Unexpected parser/runtime lines during sweep: **{0}**' -f @($report.unexpectedLines).Count))
$lines.Add("")
$lines.Add("## Flag Legend")
$lines.Add("")
$lines.Add('- Position 1: `A` = archived, space = not archived')
$lines.Add('- Position 2: `U` = userinfo, `S` = serverinfo, `C` = auto/custom, space = local/general')
$lines.Add('- Position 3: `-` = write-protected, `L` = latched, `*` = unsettable auto cvar, space = writable')
$lines.Add('- Position 4: `M` = modified/session-marked')
$lines.Add('- Position 5: `X` = ignored/hidden from normal flow')
$lines.Add("")
$lines.Add("## Invasion CVARs")
$lines.Add("")
$lines.Add("These are the invasion controls defined in source.")
$lines.Add("")
foreach ($name in ($manualDescriptions.Keys | Sort-Object)) {
    $present = if ($runtimeNameSet.Contains($name)) { "Yes" } else { "No (not in this runtime snapshot)" }
    $defaultValue = if ($runtimeNameSet.Contains($name)) {
        [string]($cvars | Where-Object { $_.Name -eq $name } | Select-Object -First 1).Value
    } else {
        [string]$invasionDefaultFallback[$name]
    }
    $range = if ($invasionRanges.ContainsKey($name)) { [string]$invasionRanges[$name] } else { "n/a" }
    $lines.Add(('### `{0}`' -f $name))
    $lines.Add("")
    $lines.Add(('- Description: {0}' -f (Get-DescriptionForCvar -Name $name)))
    $lines.Add(('- Default: `{0}`' -f $defaultValue))
    $lines.Add(('- Valid range/shape: `{0}`' -f $range))
    $lines.Add(('- Present in runtime snapshot: {0}' -f $present))
    $lines.Add("")
}

$lines.Add("## Full Runtime CVAR Catalog")
$lines.Add("")
foreach($c in $cvars) {
    $name = [string]$c.Name
    $value = [string]$c.Value
    $prefix = [string]$c.Prefix
    $flagsText = if ([string]::IsNullOrWhiteSpace($prefix)) { "(none)" } else { ('"{0}"' -f $prefix) }
    $decoded = Decode-Flags -Flags $prefix

    $tested = if ($c.IsNoSet -eq 'True') { 'No (write-protected in flag map)' } else { 'Yes (set/get sweep)' }
    $getResult = if ($c.IsNoSet -eq 'True') {
        'Not applicable'
    } elseif ($missingGetSet.Contains($name)) {
        'Missing response (investigate)'
    } else {
        'OK'
    }

    $latchObserved = if ($latchedSet.Contains($name)) { 'Yes (latched message observed during set pass)' } else { 'No' }
    $writeProtectedObserved = if ($writeProtectedSet.Contains($name)) { 'Yes' } else { 'No' }

    $lines.Add(('### `{0}`' -f $name))
    $lines.Add("")
    $lines.Add(('- Description: {0}' -f (Get-DescriptionForCvar -Name $name)))
    $lines.Add(('- Current value: `{0}`' -f $value))
    $lines.Add(('- Raw flag field: {0}' -f $flagsText))
    $lines.Add(('- Archive: {0}' -f $decoded.Archive))
    $lines.Add(('- Scope/type: {0}' -f $decoded.Scope))
    $lines.Add(('- Mutability: {0}' -f $decoded.Mutability))
    $lines.Add(('- Modified flag (`M`): {0}' -f $decoded.Mod))
    $lines.Add(('- Ignored flag (`X`): {0}' -f $decoded.Ignored))
    $lines.Add(('- Set/get tested: {0}' -f $tested))
    $lines.Add(('- Set/get result: {0}' -f $getResult))
    $lines.Add(('- Latched behavior observed: {0}' -f $latchObserved))
    $lines.Add(('- Write-protected message observed in sweep: {0}' -f $writeProtectedObserved))
    $lines.Add("")
}

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllLines($OutputPath, $lines, $utf8NoBom)

Write-Output ("Wrote {0} with {1} runtime CVAR entries and {2} invasion CVAR entries." -f $OutputPath, $cvars.Count, $manualDescriptions.Count)

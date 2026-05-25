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
$runtimeByName = @{}
foreach ($c in $cvars) {
    $runtimeName = [string]$c.Name
    [void]$runtimeNameSet.Add($runtimeName)
    $runtimeByName[$runtimeName] = $c
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

function Get-RelativeRepoPath {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $rootFull = (Resolve-Path -LiteralPath $Root).ProviderPath.TrimEnd('\', '/') + '\'
    $pathFull = (Resolve-Path -LiteralPath $Path).ProviderPath
    if ($pathFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $pathFull.Substring($rootFull.Length).Replace('\', '/')
    }
    return $pathFull.Replace('\', '/')
}

function Split-CvarMacroArguments {
    param([Parameter(Mandatory = $true)][string]$Text)

    $args = New-Object System.Collections.Generic.List[string]
    $current = New-Object System.Text.StringBuilder
    $depth = 0
    $inString = $false
    $escaped = $false

    for ($i = 0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]

        if ($inString) {
            [void]$current.Append($ch)
            if ($escaped) {
                $escaped = $false
                continue
            }
            if ($ch -eq [char]'\') {
                $escaped = $true
                continue
            }
            if ($ch -eq [char]'"') {
                $inString = $false
            }
            continue
        }

        if ($ch -eq [char]'"') {
            $inString = $true
            [void]$current.Append($ch)
            continue
        }

        if ($ch -eq [char]'(' -or $ch -eq [char]'[' -or $ch -eq [char]'{' -or $ch -eq [char]'<') {
            $depth++
        } elseif ($ch -eq [char]')' -or $ch -eq [char]']' -or $ch -eq [char]'}' -or $ch -eq [char]'>') {
            if ($depth -gt 0) {
                $depth--
            }
        }

        if ($ch -eq [char]',' -and $depth -eq 0) {
            $args.Add($current.ToString().Trim())
            [void]$current.Clear()
            continue
        }

        [void]$current.Append($ch)
    }

    $args.Add($current.ToString().Trim())
    return @($args.ToArray())
}

function Find-MatchingParenIndex {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][int]$OpenParenIndex
    )

    $depth = 0
    $inString = $false
    $escaped = $false

    for ($i = $OpenParenIndex; $i -lt $Content.Length; $i++) {
        $ch = $Content[$i]

        if ($inString) {
            if ($escaped) {
                $escaped = $false
                continue
            }
            if ($ch -eq [char]'\') {
                $escaped = $true
                continue
            }
            if ($ch -eq [char]'"') {
                $inString = $false
            }
            continue
        }

        if ($ch -eq [char]'"') {
            $inString = $true
            continue
        }
        if ($ch -eq [char]'(') {
            $depth++
            continue
        }
        if ($ch -eq [char]')') {
            $depth--
            if ($depth -eq 0) {
                return $i
            }
        }
    }

    return -1
}

function Get-LineNumberForOffset {
    param(
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][int]$Offset
    )

    if ($Offset -le 0) {
        return 1
    }

    return ([regex]::Matches($Content.Substring(0, $Offset), "`n").Count + 1)
}

function Convert-CvarDescriptionLiteral {
    param([string]$Raw)

    if ([string]::IsNullOrWhiteSpace($Raw)) {
        return ""
    }

    $parts = New-Object System.Collections.Generic.List[string]
    foreach ($m in [regex]::Matches($Raw, '"((?:\\.|[^"\\])*)"')) {
        $piece = [string]$m.Groups[1].Value
        $piece = $piece.Replace('\"', '"').Replace('\\', '\')
        $parts.Add($piece)
    }

    if ($parts.Count -gt 0) {
        return (($parts.ToArray()) -join "").Trim()
    }

    return $Raw.Trim().Trim('"')
}

function Get-SourceCvarDefinitions {
    param([string]$Root)

    $definitions = New-Object System.Collections.Generic.List[object]
    $srcRoot = Join-Path $Root "src"
    if (-not (Test-Path -LiteralPath $srcRoot)) {
        return @()
    }

    $files = Get-ChildItem -Path $srcRoot -Recurse -Include *.cpp,*.c,*.h -File
    $rxMacro = [regex]'\b(?<macro>CUSTOM_CVARD|CUSTOM_CVAR_NAMED|CUSTOM_CVAR|CVARD_NAMED|CVARD|CVAR)\s*\('

    foreach ($file in $files) {
        $content = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($m in $rxMacro.Matches($content)) {
            $lineNumber = Get-LineNumberForOffset -Content $content -Offset $m.Index
            $lineStart = $content.LastIndexOf("`n", [Math]::Max($m.Index - 1, 0))
            if ($lineStart -lt 0) {
                $lineStart = 0
            }
            $lineEnd = $content.IndexOf("`n", $m.Index)
            if ($lineEnd -lt 0) {
                $lineEnd = $content.Length
            }
            $lineText = $content.Substring($lineStart, $lineEnd - $lineStart).Trim()
            if ($lineText.StartsWith("#define")) {
                continue
            }

            $openParen = $m.Index + $m.Length - 1
            $closeParen = Find-MatchingParenIndex -Content $content -OpenParenIndex $openParen
            if ($closeParen -lt 0) {
                continue
            }

            $argsText = $content.Substring($openParen + 1, $closeParen - $openParen - 1)
            $args = Split-CvarMacroArguments -Text $argsText
            if ($args.Count -lt 4) {
                continue
            }

            $macro = [string]$m.Groups["macro"].Value
            $type = [string]$args[0]
            $name = ""
            $refName = ""
            $defaultValue = ""
            $flags = ""
            $description = ""

            if ($macro -eq "CUSTOM_CVAR_NAMED" -or $macro -eq "CVARD_NAMED") {
                if ($args.Count -lt 5) {
                    continue
                }
                $refName = [string]$args[1]
                $name = [string]$args[2]
                $defaultValue = [string]$args[3]
                $flags = [string]$args[4]
                if ($macro -eq "CVARD_NAMED" -and $args.Count -ge 6) {
                    $description = Convert-CvarDescriptionLiteral -Raw ([string]$args[5])
                }
            } elseif ($type -eq "Flag") {
                $name = [string]$args[1]
                $refName = [string]$args[1]
                $defaultValue = [string]$args[2]
                $flags = [string]$args[3]
                $description = ("Flag alias backed by `{0}`." -f $defaultValue)
            } else {
                $name = [string]$args[1]
                $refName = [string]$args[1]
                $defaultValue = [string]$args[2]
                $flags = [string]$args[3]
                if (($macro -eq "CUSTOM_CVARD" -or $macro -eq "CVARD") -and $args.Count -ge 5) {
                    $description = Convert-CvarDescriptionLiteral -Raw ([string]$args[4])
                }
            }

            $name = $name.Trim()
            $refName = $refName.Trim()
            if ([string]::IsNullOrWhiteSpace($name) -or $name.Contains("#")) {
                continue
            }

            $definitions.Add([pscustomobject]@{
                Name        = $name
                RefName     = $refName
                Type        = $type.Trim()
                Default     = $defaultValue.Trim()
                Flags       = $flags.Trim()
                Macro       = $macro
                Description = $description
                Source      = (Get-RelativeRepoPath -Root $Root -Path $file.FullName)
                Line        = $lineNumber
            })
        }
    }

    return @($definitions | Sort-Object Name, Source, Line)
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
    "sv_invasionmaxactive"        = 'Optional cap for active invasion monsters. 0 disables the cap; positive values are clamped by the engine.'
    "sv_invasionbosswaveevery"    = 'Boss wave cadence (e.g. 5 = every 5th wave, 0 = never).'
    "sv_invasionbossbonus"        = 'Extra budget added during boss waves.'
    "sv_invasionspotusemaptags"   = 'Restrict native invasion spots by map thing TID/tag. Keep disabled for Skulltag/Zandronum map compatibility; the spot arguments already control wave timing.'
    "sv_invasionspotfallback"     = 'Fallback to generic spawning when tagged invasion spots cannot be used.'
    "sv_invasionsimlod"           = 'Enables server-side simulation LOD for invasion monsters so distant actors think less often under heavy load.'
    "sv_invasionsimlodfullrange"  = 'Distance within which invasion monsters keep full-rate simulation.'
    "sv_invasionsimlodreducedrange" = 'Distance within which invasion monsters use reduced-rate simulation before becoming dormant.'
    "sv_invasionsimlodreducedinterval" = 'Think interval in tics for reduced-rate invasion simulation.'
    "sv_invasionsimloddormantinterval" = 'Think interval in tics for dormant distant invasion simulation.'
    "sv_usemapsettingswavelimit"  = 'If enabled, map-defined invasion wavelimit metadata overrides sv_invasionwaves when present.'
    "wavelimit"                   = 'Legacy Skulltag compatibility override for invasion waves. 0 disables the override; 1..255 forces that wave count.'
    "duellimit"                   = 'Legacy Skulltag compatibility value for duel limit metadata.'
    "sv_corpsequeuesize"          = 'Maximum queued corpses retained by corpse cleanup; used with sv_corpsefilter.'
    "sv_corpsefilter"             = 'Selects which corpse queues sv_corpsequeuesize trims: 0 off, 1 monsters, 2 players, 3 both.'
    "net_predict_debug"           = 'Controls HCDE prediction diagnostics: off, CSV sampling, and/or on-screen/debug trace output depending on level.'
    "net_predict_debug_interval"  = 'Tic interval used by prediction CSV/debug sampling.'
    "net_predict_softwarn_ack_lag" = 'Soft warning threshold for client ack lag during prediction diagnostics.'
    "net_predict_softwarn_mirror_delta" = 'Soft warning threshold for invasion mirror drift during prediction diagnostics.'
    "net_predict_softwarn_passive_storm" = 'Soft warning threshold for passive update storms during prediction diagnostics.'
    "net_hcde_native_only"        = 'Requires HCDE-native networking/capability paths for multiplayer sessions.'
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
    "sv_invasionmaxactive"        = "0 or 1..1024"
    "sv_invasionbosswaveevery"    = ">= 0"
    "sv_invasionbossbonus"        = ">= 0"
    "sv_invasionspotusemaptags"   = "bool"
    "sv_invasionspotfallback"     = "bool"
    "sv_invasionsimlod"           = "bool"
    "sv_invasionsimlodfullrange"  = ">= 0"
    "sv_invasionsimlodreducedrange" = ">= sv_invasionsimlodfullrange"
    "sv_invasionsimlodreducedinterval" = ">= 1 tic"
    "sv_invasionsimloddormantinterval" = ">= 1 tic"
    "sv_usemapsettingswavelimit"  = "bool"
    "wavelimit"                   = "0..255"
    "duellimit"                   = "0..255"
    "sv_corpsequeuesize"          = ">= 0"
    "sv_corpsefilter"             = "0..3"
}

$sourceDescriptions = Get-SourceDescriptions -Root $RepoRoot
$serverGuiDescriptions = Get-ServerGuiDescriptions -Root $RepoRoot
$sourceDefinitions = Get-SourceCvarDefinitions -Root $RepoRoot
$sourceByName = @{}
foreach ($definition in $sourceDefinitions) {
    if (-not $sourceByName.ContainsKey($definition.Name)) {
        $sourceByName[$definition.Name] = $definition
    }
    if (-not [string]::IsNullOrWhiteSpace($definition.Description)) {
        $sourceDescriptions[$definition.Name] = $definition.Description
    }
}

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
    "sv_invasionmaxactive"        = "0"
    "sv_invasionbosswaveevery"    = "5"
    "sv_invasionbossbonus"        = "20"
    "sv_invasionspotusemaptags"   = "0"
    "sv_invasionspotfallback"     = "1"
    "sv_invasionsimlod"           = "1"
    "sv_invasionsimlodfullrange"  = "2048"
    "sv_invasionsimlodreducedrange" = "4096"
    "sv_invasionsimlodreducedinterval" = "5"
    "sv_invasionsimloddormantinterval" = "TICRATE * 3"
    "sv_usemapsettingswavelimit"  = "1"
    "wavelimit"                   = "0"
    "duellimit"                   = "0"
    "sv_corpsequeuesize"          = "64"
    "sv_corpsefilter"             = "1"
}

$stampUtc = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss")
$sourceNameSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($definition in $sourceDefinitions) {
    [void]$sourceNameSet.Add([string]$definition.Name)
}

$sourceOnlyCount = 0
foreach ($definition in $sourceByName.Values) {
    if (-not $runtimeNameSet.Contains([string]$definition.Name)) {
        $sourceOnlyCount++
    }
}

$runtimeOnlyCount = 0
foreach ($c in $cvars) {
    if (-not $sourceNameSet.Contains([string]$c.Name)) {
        $runtimeOnlyCount++
    }
}

$hcdeFocusSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($name in $manualDescriptions.Keys) {
    [void]$hcdeFocusSet.Add([string]$name)
}
foreach ($definition in $sourceDefinitions) {
    $name = [string]$definition.Name
    if ($name.StartsWith("sv_invasion", [System.StringComparison]::OrdinalIgnoreCase) -or
        $name.StartsWith("net_predict_", [System.StringComparison]::OrdinalIgnoreCase)) {
        [void]$hcdeFocusSet.Add($name)
    }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# HCDE CVAR Reference")
$lines.Add("")
$lines.Add(("Generated: {0} UTC" -f $stampUtc))
$lines.Add("")
$lines.Add('This reference combines source-defined CVAR inventory with the imported runtime audit snapshot.')
$lines.Add("")
$lines.Add("## Coverage")
$lines.Add("")
$lines.Add(('- Source CVAR definitions discovered: **{0}** unique / **{1}** total macro definitions' -f $sourceByName.Count, $sourceDefinitions.Count))
$lines.Add(('- Source-defined CVARs absent from imported runtime snapshot: **{0}**' -f $sourceOnlyCount))
$lines.Add(('- Runtime-only CVARs from imported snapshot: **{0}**' -f $runtimeOnlyCount))
$lines.Add(('- Total runtime CVARs in imported snapshot: **{0}**' -f $report.parsedCvars))
$lines.Add(('- Set/get tested runtime CVARs: **{0}**' -f $report.setTargets))
$lines.Add(('- Successful get responses: **{0}**' -f $report.getResponses))
$lines.Add(('- Missing get responses: **{0}**' -f @($report.missingGetResponses).Count))
$lines.Add(('- Unexpected parser/runtime lines during sweep: **{0}**' -f @($report.unexpectedLines).Count))
$lines.Add(('- Runtime baseline CSV: `{0}`' -f $BaselineCsv))
$lines.Add(('- Set/get report: `{0}`' -f $SetpassReport))
$lines.Add("")
$lines.Add("> Note: the source catalog is regenerated from the current checkout. The runtime snapshot is imported from the audit files above, so entries marked absent may still be valid source CVARs that were not visible in that older runtime capture.")
$lines.Add("")
$lines.Add("## Flag Legend")
$lines.Add("")
$lines.Add('- Position 1: `A` = archived, space = not archived')
$lines.Add('- Position 2: `U` = userinfo, `S` = serverinfo, `C` = auto/custom, space = local/general')
$lines.Add('- Position 3: `-` = write-protected, `L` = latched, `*` = unsettable auto cvar, space = writable')
$lines.Add('- Position 4: `M` = modified/session-marked')
$lines.Add('- Position 5: `X` = ignored/hidden from normal flow')
$lines.Add("")
$lines.Add("## HCDE Server, Invasion, and Netcode CVARs")
$lines.Add("")
$lines.Add("These are the high-value controls for invasion, net diagnostics, compatibility, and heavy-load cleanup.")
$lines.Add("")
foreach ($name in (@($hcdeFocusSet) | Sort-Object)) {
    $present = if ($runtimeNameSet.Contains($name)) { "Yes" } else { "No (not in this runtime snapshot)" }
    $sourceDefinition = if ($sourceByName.ContainsKey($name)) { $sourceByName[$name] } else { $null }
    $defaultValue = if ($null -ne $sourceDefinition) {
        [string]$sourceDefinition.Default
    } elseif ($invasionDefaultFallback.ContainsKey($name)) {
        [string]$invasionDefaultFallback[$name]
    } elseif ($runtimeNameSet.Contains($name)) {
        [string]$runtimeByName[$name].Value
    } else {
        "n/a"
    }
    $range = if ($invasionRanges.ContainsKey($name)) { [string]$invasionRanges[$name] } else { "n/a" }
    $sourceText = if ($null -ne $sourceDefinition) { ("{0}:{1}" -f $sourceDefinition.Source, $sourceDefinition.Line) } else { "Not found in source scan" }
    $runtimeValue = if ($runtimeNameSet.Contains($name)) { [string]$runtimeByName[$name].Value } else { "n/a" }
    $lines.Add(('### `{0}`' -f $name))
    $lines.Add("")
    $lines.Add(('- Description: {0}' -f (Get-DescriptionForCvar -Name $name)))
    $lines.Add(('- Source default: `{0}`' -f $defaultValue))
    $lines.Add(('- Valid range/shape: `{0}`' -f $range))
    $lines.Add(('- Source: `{0}`' -f $sourceText))
    $lines.Add(('- Present in runtime snapshot: {0}' -f $present))
    $lines.Add(('- Runtime snapshot value: `{0}`' -f $runtimeValue))
    $lines.Add("")
}

$lines.Add("## Source-Defined CVAR Catalog")
$lines.Add("")
$lines.Add("This section is generated from `CVAR`, `CUSTOM_CVAR`, `CVARD`, `CUSTOM_CVARD`, and named CVAR macros in `src/`.")
$lines.Add("")
foreach ($definition in ($sourceByName.Values | Sort-Object Name)) {
    $name = [string]$definition.Name
    $runtimePresent = if ($runtimeNameSet.Contains($name)) { "Yes" } else { "No" }
    $runtimeValue = if ($runtimeNameSet.Contains($name)) { [string]$runtimeByName[$name].Value } else { "n/a" }
    $refText = if ($definition.RefName -ne $definition.Name) { [string]$definition.RefName } else { "same as cvar name" }

    $lines.Add(('### `{0}`' -f $name))
    $lines.Add("")
    $lines.Add(('- Description: {0}' -f (Get-DescriptionForCvar -Name $name)))
    $lines.Add(('- Type: `{0}`' -f $definition.Type))
    $lines.Add(('- Source default: `{0}`' -f $definition.Default))
    $lines.Add(('- Source flags: `{0}`' -f $definition.Flags))
    $lines.Add(('- Macro: `{0}`' -f $definition.Macro))
    $lines.Add(('- Ref symbol: `{0}`' -f $refText))
    $lines.Add(('- Source: `{0}:{1}`' -f $definition.Source, $definition.Line))
    $lines.Add(('- Present in runtime snapshot: {0}' -f $runtimePresent))
    $lines.Add(('- Runtime snapshot value: `{0}`' -f $runtimeValue))
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

Write-Output ("Wrote {0} with {1} source CVAR entries and {2} imported runtime CVAR entries." -f $OutputPath, $sourceByName.Count, $cvars.Count)

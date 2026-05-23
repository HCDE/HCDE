param(
    [string]$EngineDir = "",
    [string]$Iwad = "C:\Users\user\Downloads\doom2.wad",
    [string]$OutputDir = "",
    [int]$MaxCvars = 0,
    [switch]$SkipSetPass
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Resolve-Path -LiteralPath $Path).ProviderPath
}

function New-Utf8NoBomEncoding {
    return [System.Text.UTF8Encoding]::new($false)
}

function Write-LinesUtf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string[]]$Lines
    )

    [System.IO.File]::WriteAllLines($Path, $Lines, (New-Utf8NoBomEncoding))
}

function Get-MarkerLines {
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$StartMarker,
        [Parameter(Mandatory = $true)][string]$EndMarker
    )

    $startIndex = -1
    $endIndex = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        if ($startIndex -lt 0 -and $line.Contains($StartMarker)) {
            $startIndex = $i
            continue
        }
        if ($startIndex -ge 0 -and $line.Contains($EndMarker)) {
            $endIndex = $i
            break
        }
    }

    if ($startIndex -lt 0) {
        throw "Could not find start marker '$StartMarker' in log output."
    }
    if ($endIndex -lt 0) {
        throw "Could not find end marker '$EndMarker' in log output."
    }
    if ($endIndex -le ($startIndex + 1)) {
        return @()
    }

    return $Lines[($startIndex + 1)..($endIndex - 1)]
}

function Parse-CvarsFromLines {
    param([Parameter(Mandatory = $true)][string[]]$Lines)

    $cvarPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+(?<prefix>.*?)(?<name>[A-Za-z_][A-Za-z0-9_]*)\s"(?<value>.*)"$'
    $countPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+(?<count>\d+)\s+cvars$'

    $byName = [ordered]@{}
    $declaredCount = $null

    foreach ($line in $Lines) {
        if ($line -match $countPattern) {
            $declaredCount = [int]$Matches['count']
            continue
        }

        if ($line -notmatch $cvarPattern) {
            continue
        }

        $name = [string]$Matches['name']
        $value = [string]$Matches['value']
        $prefix = [string]$Matches['prefix']
        $flagField = $prefix.TrimEnd()

        if (-not $byName.Contains($name)) {
            $byName[$name] = [pscustomobject]@{
                Name         = $name
                Value        = $value
                Prefix       = $prefix
                Flags        = $flagField
                IsNoSet      = $flagField.Contains("-")
                IsLatched    = $flagField.Contains("L")
                IsUnsettable = $flagField.Contains("*")
            }
        }
    }

    return [pscustomobject]@{
        Cvars         = @($byName.Values)
        DeclaredCount = $declaredCount
    }
}

function Invoke-HcdeServerWithScript {
    param(
        [Parameter(Mandatory = $true)][string]$EngineDir,
        [Parameter(Mandatory = $true)][string]$ServerExe,
        [Parameter(Mandatory = $true)][string]$Iwad,
        [Parameter(Mandatory = $true)][string[]]$ScriptLines,
        [Parameter(Mandatory = $true)][string]$ScriptTag,
        [Parameter(Mandatory = $true)][string]$RunLogCopyPath
    )

    $cfgName = "tmp_cvar_audit_$ScriptTag.cfg"
    $cfgPath = Join-Path $EngineDir $cfgName
    $runOutputPath = Join-Path $EngineDir "tmp-hcde-cvar-audit-std.log"
    $runErrorPath = Join-Path $EngineDir "tmp-hcde-cvar-audit-err.log"

    Write-LinesUtf8NoBom -Path $cfgPath -Lines $ScriptLines
    if (Test-Path -LiteralPath $runOutputPath) {
        Remove-Item -LiteralPath $runOutputPath -Force
    }
    if (Test-Path -LiteralPath $runErrorPath) {
        Remove-Item -LiteralPath $runErrorPath -Force
    }

    $serverExeLeaf = [System.IO.Path]::GetFileName($ServerExe)
    $serverArgs = @(
        "-iwad", $Iwad,
        "+set", "sv_upnp", "0",
        "+set", "sv_usemasters", "0",
        "+exec", $cfgName
    )

    Push-Location $EngineDir
    try {
        $proc = Start-Process -FilePath (Join-Path $EngineDir $serverExeLeaf) `
            -ArgumentList $serverArgs `
            -WorkingDirectory $EngineDir `
            -RedirectStandardOutput $runOutputPath `
            -RedirectStandardError $runErrorPath `
            -PassThru -Wait -NoNewWindow
        $exitCode = if ($proc -ne $null -and -not $proc.HasExited) { 255 } else { $proc.ExitCode }
    }
    finally {
        Pop-Location
        if (Test-Path -LiteralPath $cfgPath) {
            Remove-Item -LiteralPath $cfgPath -Force
        }
    }

    if (-not (Test-Path -LiteralPath $runOutputPath)) {
        if (Test-Path -LiteralPath $runErrorPath) {
            Copy-Item -LiteralPath $runErrorPath -Destination $RunLogCopyPath -Force
        }
        throw "HCDE server run completed without producing output capture file."
    }

    if (Test-Path -LiteralPath $runErrorPath) {
        $errLen = (Get-Item -LiteralPath $runErrorPath).Length
        if ($errLen -gt 0) {
            Add-Content -Path $runOutputPath -Value "" 
            Add-Content -Path $runOutputPath -Value "=== STDERR ==="
            Get-Content -LiteralPath $runErrorPath | Add-Content -Path $runOutputPath
        }
    }

    Copy-Item -LiteralPath $runOutputPath -Destination $RunLogCopyPath -Force
    if (Test-Path -LiteralPath $runErrorPath) {
        Remove-Item -LiteralPath $runErrorPath -Force
    }
    Remove-Item -LiteralPath $runOutputPath -Force
    return [int]$exitCode
}

$hcdeRoot = Resolve-FullPath -Path (Join-Path $PSScriptRoot "..")
$workspaceRoot = Resolve-FullPath -Path (Join-Path $hcdeRoot "..")

if ([string]::IsNullOrWhiteSpace($EngineDir)) {
    $EngineDir = Join-Path $workspaceRoot "build-output\hcde-local-test-engine-x64"
}
$EngineDir = Resolve-FullPath -Path $EngineDir

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDir = Join-Path $hcdeRoot "test-logs\cvar-audit\$stamp"
}
if (-not (Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}
$OutputDir = Resolve-FullPath -Path $OutputDir

$CandidateServers = @(
    (Join-Path -Path $EngineDir -ChildPath "HCDE.exe"),
    (Join-Path -Path $EngineDir -ChildPath "HCDEserv.exe")
)
$ServerExe = $null
foreach ($candidate in $CandidateServers) {
    if (Test-Path -LiteralPath $candidate) {
        $ServerExe = (Resolve-Path -LiteralPath $candidate).ProviderPath
        break
    }
}
if ($null -eq $ServerExe -or -not (Test-Path -LiteralPath $ServerExe)) {
    throw "No HCDE executable found in $EngineDir (expected HCDE.exe or HCDEserv.exe)."
}
if (-not (Test-Path -LiteralPath $Iwad)) {
    throw "IWAD file not found: $Iwad"
}

Write-Host "HCDE CVAR audit starting..." -ForegroundColor Cyan
Write-Host "EngineDir: $EngineDir"
Write-Host "IWAD:      $Iwad"
Write-Host "OutputDir: $OutputDir"

$baselineRunLog = Join-Path $OutputDir "run-baseline.log"
$baselineScript = @(
    "echo CVAR_AUDIT_BASELINE_BEGIN",
    "cvarlist",
    "echo CVAR_AUDIT_BASELINE_END",
    "quit"
)

$baselineExit = Invoke-HcdeServerWithScript `
    -EngineDir $EngineDir `
    -ServerExe $ServerExe `
    -Iwad $Iwad `
    -ScriptLines $baselineScript `
    -ScriptTag "baseline" `
    -RunLogCopyPath $baselineRunLog

$baselineLines = Get-Content -LiteralPath $baselineRunLog
$baselineCvarLines = Get-MarkerLines -Lines $baselineLines -StartMarker "CVAR_AUDIT_BASELINE_BEGIN" -EndMarker "CVAR_AUDIT_BASELINE_END"
$parsedBaseline = Parse-CvarsFromLines -Lines $baselineCvarLines
$allCvars = @($parsedBaseline.Cvars)

if ($allCvars.Count -eq 0) {
    throw "No CVAR entries were parsed from baseline cvarlist output."
}

$declared = if ($null -eq $parsedBaseline.DeclaredCount) { -1 } else { [int]$parsedBaseline.DeclaredCount }

$allCvars | Sort-Object Name | Export-Csv -LiteralPath (Join-Path $OutputDir "baseline-cvars.csv") -NoTypeInformation

$setCandidates = @($allCvars | Where-Object { -not $_.IsNoSet })
if ($MaxCvars -gt 0) {
    $setCandidates = @($setCandidates | Select-Object -First $MaxCvars)
}

$report = [ordered]@{
    generatedAtUtc         = (Get-Date).ToUniversalTime().ToString("o")
    engineDir              = $EngineDir
    iwad                   = $Iwad
    baselineExitCode       = $baselineExit
    declaredCvarCount      = $declared
    parsedCvarCount        = $allCvars.Count
    noSetCvarCount         = @($allCvars | Where-Object { $_.IsNoSet }).Count
    latchedCvarCount       = @($allCvars | Where-Object { $_.IsLatched }).Count
    setCandidateCount      = $setCandidates.Count
    setPassExecuted        = (-not $SkipSetPass.IsPresent)
    setPassExitCode        = $null
    missingGetResponses    = @()
    unsetResponses         = @()
    unexpectedWriteProtect = @()
    unexpectedLatched      = @()
    unexpectedLines        = @()
}

if (-not $SkipSetPass.IsPresent) {
    $setScriptLines = New-Object System.Collections.Generic.List[string]
    $setScriptLines.Add("echo CVAR_AUDIT_SET_BEGIN")

    foreach ($cvar in $setCandidates) {
        $escapedValue = $cvar.Value.Replace('"', '\"')
        $setScriptLines.Add(("set {0} ""{1}""" -f $cvar.Name, $escapedValue))
        $setScriptLines.Add(("get {0}" -f $cvar.Name))
    }

    $setScriptLines.Add("echo CVAR_AUDIT_SET_END")
    $setScriptLines.Add("quit")

    $setRunLog = Join-Path $OutputDir "run-setpass.log"
    $setExit = Invoke-HcdeServerWithScript `
        -EngineDir $EngineDir `
        -ServerExe $ServerExe `
        -Iwad $Iwad `
        -ScriptLines $setScriptLines.ToArray() `
        -ScriptTag "setpass" `
        -RunLogCopyPath $setRunLog

    $report.setPassExitCode = $setExit

    $setLines = Get-Content -LiteralPath $setRunLog
    $setSection = Get-MarkerLines -Lines $setLines -StartMarker "CVAR_AUDIT_SET_BEGIN" -EndMarker "CVAR_AUDIT_SET_END"

    $getPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+"(?<name>[A-Za-z_][A-Za-z0-9_]*)"\s+is\s+"(?<value>.*)"$'
    $unsetPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+"(?<name>[A-Za-z_][A-Za-z0-9_]*)"\s+is\s+unset$'
    $writeProtectPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+(?<name>[A-Za-z_][A-Za-z0-9_]*) is write protected\.$'
    $latchedPattern = '^\[\d{2}:\d{2}:\d{2}\]\s+(?<name>[A-Za-z_][A-Za-z0-9_]*) will be changed for next game\.$'

    $getResults = @{}
    $writeProtectedNames = New-Object System.Collections.Generic.List[string]
    $latchedNames = New-Object System.Collections.Generic.List[string]
    $unsetNames = New-Object System.Collections.Generic.List[string]
    $unexpected = New-Object System.Collections.Generic.List[string]

    foreach ($line in $setSection) {
        if ($line -match $getPattern) {
            $getResults[[string]$Matches['name']] = [string]$Matches['value']
            continue
        }
        if ($line -match $unsetPattern) {
            $unsetNames.Add([string]$Matches['name'])
            continue
        }
        if ($line -match $writeProtectPattern) {
            $writeProtectedNames.Add([string]$Matches['name'])
            continue
        }
        if ($line -match $latchedPattern) {
            $latchedNames.Add([string]$Matches['name'])
            continue
        }
        if ($line -match 'usage: set|usage: get|Unknown command|No such variable|ERROR:') {
            $unexpected.Add($line)
        }
    }

    $missingGets = @()
    foreach ($cvar in $setCandidates) {
        if (-not $getResults.ContainsKey($cvar.Name)) {
            $missingGets += $cvar.Name
        }
    }

    $cvarByName = @{}
    foreach ($cvar in $allCvars) {
        $cvarByName[$cvar.Name] = $cvar
    }

    $unexpectedWrite = @()
    foreach ($name in $writeProtectedNames) {
        if ($cvarByName.ContainsKey($name) -and -not $cvarByName[$name].IsNoSet) {
            $unexpectedWrite += $name
        }
    }

    $unexpectedLatch = @()
    foreach ($name in $latchedNames) {
        if ($cvarByName.ContainsKey($name) -and -not $cvarByName[$name].IsLatched) {
            $unexpectedLatch += $name
        }
    }

    $report.missingGetResponses = @($missingGets | Sort-Object -Unique)
    $report.unsetResponses = @($unsetNames.ToArray() | Sort-Object -Unique)
    $report.unexpectedWriteProtect = @($unexpectedWrite | Sort-Object -Unique)
    $report.unexpectedLatched = @($unexpectedLatch | Sort-Object -Unique)
    $report.unexpectedLines = @($unexpected.ToArray() | Sort-Object -Unique)
}

$reportPath = Join-Path $OutputDir "cvar-audit-report.json"
$summaryPath = Join-Path $OutputDir "SUMMARY.txt"

$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8

$summaryLines = @(
    ("Generated (UTC): {0}" -f $report.generatedAtUtc),
    ("Baseline exit code: {0}" -f $report.baselineExitCode),
    ("Declared CVAR count: {0}" -f $report.declaredCvarCount),
    ("Parsed CVAR count:   {0}" -f $report.parsedCvarCount),
    ("No-set CVAR count:   {0}" -f $report.noSetCvarCount),
    ("Latched CVAR count:  {0}" -f $report.latchedCvarCount),
    ("Set candidates:      {0}" -f $report.setCandidateCount),
    ("Set pass executed:   {0}" -f $report.setPassExecuted),
    ("Set pass exit code:  {0}" -f $report.setPassExitCode),
    ("Missing get replies: {0}" -f (@($report.missingGetResponses).Count)),
    ("Unset replies:       {0}" -f (@($report.unsetResponses).Count)),
    ("Unexpected write-protect replies: {0}" -f (@($report.unexpectedWriteProtect).Count)),
    ("Unexpected latched replies:       {0}" -f (@($report.unexpectedLatched).Count)),
    ("Unexpected output lines:          {0}" -f (@($report.unexpectedLines).Count)),
    "",
    ("Report:  {0}" -f $reportPath),
    ("CSV:     {0}" -f (Join-Path $OutputDir "baseline-cvars.csv")),
    ("Baseline log: {0}" -f $baselineRunLog)
)

if (-not $SkipSetPass.IsPresent) {
    $summaryLines += ("Set pass log: {0}" -f (Join-Path $OutputDir "run-setpass.log"))
}

Write-LinesUtf8NoBom -Path $summaryPath -Lines $summaryLines

Write-Host ""
Write-Host "HCDE CVAR audit complete." -ForegroundColor Green
Get-Content -LiteralPath $summaryPath | ForEach-Object { Write-Host $_ }

$hasIssues =
    (@($report.missingGetResponses).Count -gt 0) -or
    (@($report.unsetResponses).Count -gt 0) -or
    (@($report.unexpectedWriteProtect).Count -gt 0) -or
    (@($report.unexpectedLatched).Count -gt 0) -or
    (@($report.unexpectedLines).Count -gt 0)

if ($hasIssues) {
    exit 2
}

exit 0

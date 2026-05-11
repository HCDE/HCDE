param(
    [string]$Remote = "gitea",
    [string]$Branch = "main",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$GitArgs)

    & git @GitArgs
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArgs -join ' ') failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
Set-Location -LiteralPath $repoRoot

$currentBranch = (& git branch --show-current).Trim()
if ($currentBranch -ne $Branch) {
    throw "Refusing to sync branch '$currentBranch' to '$Branch'. Checkout $Branch first or pass -Branch $currentBranch."
}

$remoteUrl = (& git remote get-url $Remote 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($remoteUrl)) {
    throw "Remote '$Remote' is not configured."
}

Invoke-Git fetch $Remote $Branch

$localHead = (& git rev-parse HEAD).Trim()
$remoteRef = (& git rev-parse --verify --quiet "$Remote/$Branch").Trim()
if (-not [string]::IsNullOrWhiteSpace($remoteRef)) {
    $mergeBase = (& git merge-base HEAD "$Remote/$Branch").Trim()
    if ($mergeBase -ne $remoteRef) {
        throw "Remote '$Remote/$Branch' contains commits not present locally. Pull or merge before syncing."
    }
}

Write-Host "Syncing $localHead to $Remote/$Branch ($remoteUrl)"
if ($DryRun) {
    Write-Host "Dry run only; no push performed."
    exit 0
}

Invoke-Git push $Remote "$Branch`:refs/heads/$Branch"
Write-Host "Gitea sync complete."

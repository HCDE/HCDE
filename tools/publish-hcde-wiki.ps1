# Publishes wiki/*.md to https://github.com/bokoxthexchocobo/HCDE/wiki
# Requires git credentials with push access to the HCDE.wiki repository.
param(
    [string]$RepoRoot = "",
    [string]$WikiRemote = "https://github.com/bokoxthexchocobo/HCDE.wiki.git"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).ProviderPath
}

$wikiSource = Join-Path $RepoRoot "wiki"
if (-not (Test-Path -LiteralPath $wikiSource)) {
    throw "Wiki source folder not found: $wikiSource"
}

$workDir = Join-Path ([System.IO.Path]::GetTempPath()) ("hcde-wiki-publish-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

try {
    Push-Location $workDir
    git init -b main | Out-Null
    Copy-Item -Path (Join-Path $wikiSource "*.md") -Destination $workDir -Force
    git add *.md
    git -c user.email="hcde-wiki@users.noreply.github.com" -c user.name="HCDE Wiki Publish" commit -m "Sync wiki from HCDE repository" | Out-Null
    git remote add origin $WikiRemote
    git push -u origin main --force
    Write-Host "Wiki published to $WikiRemote"
}
finally {
    Pop-Location
    Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
}

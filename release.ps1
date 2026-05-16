# Usage: .\release.ps1 v0.2.0 "Description de la release"
param(
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$false)][string]$Notes = ""
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# 0. Ensure gh (GitHub CLI) is on PATH
$ghDirs = @(
    "C:\Program Files\GitHub CLI",
    "C:\Program Files (x86)\GitHub CLI",
    "$env:LOCALAPPDATA\Programs\GitHub CLI"
)
foreach ($d in $ghDirs) {
    if ((Test-Path (Join-Path $d "gh.exe")) -and ($env:PATH -notlike "*$d*")) {
        $env:PATH = "$d;$env:PATH"
    }
}

$Env  = "mhetesp32minikit"
$Bin  = ".pio\build\$Env\firmware.bin"

# 1. Commit any pending changes
git add -A
$status = git status --porcelain
if ($status) {
    git commit -m "$Version"
}

# 2. Create tag
git tag -a $Version -m $Version
Write-Host "Tag $Version cree" -ForegroundColor Green

# 3. Push commit + tag
git push origin master $Version
Write-Host "Push OK" -ForegroundColor Green

# 4. Rebuild (so FW_RELEASE picks up the new tag)
pio run
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
Write-Host "Build OK" -ForegroundColor Green

# 5. Create GitHub release with firmware
$notesArg = if ($Notes) { $Notes } else { $Version }
gh release create $Version --title $Version --notes $notesArg $Bin
Write-Host "Release $Version creee" -ForegroundColor Green

# 6. Re-upload the binary (built AFTER tag, so FW_RELEASE is correct)
gh release upload $Version $Bin --clobber
Write-Host "Firmware mis a jour dans la release" -ForegroundColor Green

Write-Host "`nTermine! Release $Version disponible sur GitHub." -ForegroundColor Cyan

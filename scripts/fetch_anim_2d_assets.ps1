param(
    [Parameter(Mandatory = $true)]
    [string]$Path
)

$ErrorActionPreference = "Stop"

$rootDir = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$targetDir = Join-Path $rootDir "examples\anim_2d\assets"

function Show-Usage {
    Write-Host "Usage: scripts\fetch_anim_2d_assets.ps1 -Path C:\path\to\craftpix_pack.zip"
    Write-Host "   or: scripts\fetch_anim_2d_assets.ps1 -Path C:\path\to\unzipped_pack_dir"
}

if (-not (Test-Path $Path)) {
    Show-Usage
    throw "Path not found: $Path"
}

$baseDir = $null

if (Test-Path $Path -PathType Container) {
    $baseDir = (Resolve-Path $Path).Path
} else {
    $tmpDir = Join-Path $env:TEMP ([System.Guid]::NewGuid().ToString())
    New-Item -ItemType Directory -Path $tmpDir | Out-Null
    try {
        Expand-Archive -Path $Path -DestinationPath $tmpDir -Force
    } catch {
        Remove-Item -Recurse -Force $tmpDir
        throw $_
    }

    $idle = Get-ChildItem -Path $tmpDir -Recurse -Filter "Idle.png" |
        Where-Object { $_.FullName -match "\\1\\Idle\.png$" } |
        Select-Object -First 1
    if (-not $idle) {
        Remove-Item -Recurse -Force $tmpDir
        throw "Could not find expected '1\Idle.png' in the zip."
    }
    $baseDir = (Resolve-Path (Join-Path $idle.Directory.FullName "..")).Path
}

if (-not (Test-Path (Join-Path $baseDir "1\Idle.png"))) {
    throw "Expected a folder containing 1..6 sprite directories in: $baseDir"
}

New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

1..6 | ForEach-Object {
    $variantDir = Join-Path $baseDir $_
    if (Test-Path $variantDir) {
        $destDir = Join-Path $targetDir $_
        if (Test-Path $destDir) {
            Remove-Item -Recurse -Force $destDir
        }
        Copy-Item -Recurse -Force $variantDir $targetDir
    } else {
        Write-Warning "Missing variant folder $_ in $baseDir"
    }
}

Write-Host "Assets installed to $targetDir"

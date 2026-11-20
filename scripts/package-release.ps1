param(
    [string]$Version = "0.3.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

& cmd.exe /c "scripts\build-win7-x86.cmd"
if ($LASTEXITCODE -ne 0) {
    throw "Win7 build/gate failed."
}

$exe = Join-Path $root "dist\ZaYuCursorHalo.exe"
if (-not (Test-Path $exe)) {
    throw "Missing $exe"
}

$releaseDir = Join-Path $root "release"
$stage = Join-Path $releaseDir "ZaYuCursorHalo-v$Version-win7-x86"
$zip = Join-Path $releaseDir "ZaYuCursorHalo-v$Version-win7-x86.zip"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item $exe (Join-Path $stage "ZaYuCursorHalo.exe")
foreach ($file in @("README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md")) {
    Copy-Item (Join-Path $root $file) (Join-Path $stage $file)
}
New-Item -ItemType Directory -Path (Join-Path $stage "docs") -Force | Out-Null
Get-ChildItem (Join-Path $root "docs") -Filter "*.md" | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $stage "docs\$($_.Name)")
}
Copy-Item (Join-Path $root "artifacts") (Join-Path $stage "artifacts") -Recurse

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash
$size = (Get-Item $zip).Length

Write-Host ""
Write-Host "Release package: $zip"
Write-Host "SHA256: $hash"
Write-Host ("Size: {0:N0} bytes" -f $size)
Write-Host "Attach this zip to the GitHub release; do not commit release/."

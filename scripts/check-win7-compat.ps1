param(
    [string]$Exe = "dist\CursorHalo.exe",
    [string]$VsPath = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($Exe)) {
    $Exe = Join-Path $root $Exe
}
$Exe = [System.IO.Path]::GetFullPath($Exe)
if (-not (Test-Path $Exe)) {
    throw "Executable not found: $Exe"
}

if (-not $VsPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found."
    }
    $VsPath = (& $vswhere -products * -version "[17.0,18.0)" -property installationPath | Select-Object -First 1)
}
if (-not $VsPath) {
    throw "Visual Studio 2022 Build Tools not found."
}

$vcvars = Join-Path $VsPath "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvarsall.bat not found under $VsPath."
}

function Invoke-Dumpbin([string]$Mode, [string]$File)
{
    $command = "call `"$vcvars`" x86 >nul && dumpbin $Mode `"$File`""
    $output = cmd.exe /c $command 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin $Mode failed for $File"
    }
    return $output
}

$imports = Invoke-Dumpbin "/imports" $Exe
$headers = Invoke-Dumpbin "/headers" $Exe

$failures = New-Object System.Collections.Generic.List[string]

# Win7 has no API-set forwarders, no UCRT, and no VC++ redistributable here.
$bannedImports = @(
    "MAGNIFICATION\.dll",
    "SHCORE\.dll",
    "DCOMP\.dll",
    "D3D11_2\.dll",
    "api-ms-win-",
    "ext-ms-win-",
    "VCRUNTIME",
    "MSVCP",
    "ucrtbase"
)
foreach ($pattern in $bannedImports) {
    if ($imports -match "(?im)$pattern") {
        $failures.Add("Forbidden import matched: $pattern")
    }
}

# Exports that are Win8+ only or otherwise unsafe as static imports on Win7.
$bannedExports = @(
    "MagShowSystemCursor",
    "MagSetFullscreenTransform",
    "GetDpiForWindow",
    "SetProcessDpiAwarenessContext",
    "SetThreadDpiAwarenessContext",
    "GetSystemMetricsForDpi",
    "AdjustWindowRectExForDpi",
    "EnableNonClientDpiScaling",
    "SetProcessDpiAwareness",
    "GetDpiForMonitor",
    "SetCoalescableTimer",
    "GetPointerType"
)
foreach ($name in $bannedExports) {
    if ($imports -match "(?im)\b$([regex]::Escape($name))\b") {
        $failures.Add("Forbidden static import: $name")
    }
}

if ($headers -notmatch "(?im)machine\s*\(x86\)") {
    $failures.Add("Architecture is not x86.")
}

$subsystemMatch = [regex]::Match($headers, "(?im)^\s*([0-9.]+)\s+subsystem version\s*$")
if (-not $subsystemMatch.Success) {
    $failures.Add("Cannot read subsystem version.")
} else {
    $subsystem = [version]$subsystemMatch.Groups[1].Value
    if ($subsystem -gt [version]"6.1") {
        $failures.Add("Subsystem version $subsystem is higher than 6.01 (Windows 7).")
    }
}

Write-Host "Win7 compatibility gate: $Exe"
$dlls = [regex]::Matches($imports, "(?im)^\s+([A-Za-z0-9_.-]+\.dll)\s*$") |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique
Write-Host ("Imports: " + ($dlls -join ", "))

if ($failures.Count -gt 0) {
    Write-Host "FAILED:"
    foreach ($failure in $failures) {
        Write-Host "  - $failure"
    }
    exit 1
}

Write-Host "PASS: x86, subsystem <= 6.01, no Win7-forbidden static imports."
exit 0

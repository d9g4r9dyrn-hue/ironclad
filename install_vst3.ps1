# Copies the built Ironclad VST3 to the system plugins folder.
# Requires admin rights - run this in an elevated PowerShell window.

$ErrorActionPreference = "Stop"

$source = "C:\Users\cort\Desktop\ironclad\build\Ironclad_artefacts\Release\VST3\Ironclad.vst3"
$destDir = "C:\Program Files\Common Files\VST3"
$dest = Join-Path $destDir "Ironclad.vst3"

if (-not (Test-Path $source)) {
    Write-Error "Built VST3 not found at $source. Build the Release configuration first."
    exit 1
}

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Not running as Administrator. Right-click this script and 'Run with PowerShell' as admin, or run from an elevated prompt."
    exit 1
}

if (Test-Path $dest) {
    Remove-Item -Recurse -Force $dest
}

Copy-Item -Recurse -Force $source $dest

Write-Host "Installed Ironclad.vst3 to $destDir"

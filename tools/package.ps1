# Builds the distributable Ironclad zip for Gumroad: VST3 bundle + standalone
# exe + README, staged into a clean folder and compressed. No arguments so the
# invocation string stays constant/allowlisted.

$ErrorActionPreference = "Stop"
$version = "0.1.0"
$root    = "C:\Users\cort\Desktop\ironclad"
$rel     = "$root\build\Ironclad_artefacts\Release"
$dist    = "$root\dist"
$stage   = "$dist\Ironclad"
$zip     = "$dist\Ironclad-v$version-Windows.zip"

# Fresh staging folder.
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# VST3 bundle (a folder) + standalone exe + readme.
Copy-Item "$rel\VST3\Ironclad.vst3" $stage -Recurse -Force
Copy-Item "$rel\Standalone\Ironclad.exe" $stage -Force
if (Test-Path "$dist\README.txt") { Copy-Item "$dist\README.txt" $stage -Force }

# Zip the staged folder (so the archive contains a single Ironclad\ root).
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal

"Packaged: $zip"
"{0:N2} MB" -f ((Get-Item $zip).Length / 1MB)
Get-ChildItem $stage -Recurse | Where-Object { -not $_.PSIsContainer } | ForEach-Object { $_.FullName.Replace($stage, "  Ironclad") }

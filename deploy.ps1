# Deploy Ironclad.vst3 to FL's VST3 folder.
#
# The critical step is DELETING the destination bundle before copying. If the
# dest dir already exists, `Copy-Item -Recurse "src\Ironclad.vst3" "dst\Ironclad.vst3"`
# copies the source folder as a CHILD (dst\Ironclad.vst3\Ironclad.vst3\...) and
# leaves the top-level DLL FL loads untouched -- so every rebuild after the first
# silently fails to reach FL. `-Force` does not fix this. Deleting first makes the
# copy create the bundle correctly. (Closing FL is also good hygiene: a loaded DLL
# can't be overwritten.) Finally we verify src/dst DLL sizes match.

$ErrorActionPreference = 'Stop'

$src = "C:\Users\cort\Desktop\corticorp\plugins\ironclad\build\Ironclad_artefacts\Release\VST3\Ironclad.vst3"
$dst = "C:\Program Files\Common Files\VST3\Ironclad.vst3"
$dll = "Contents\x86_64-win\Ironclad.vst3"

if (-not (Test-Path $src)) { throw "Build output not found: $src  (build Release first)" }

# 1. Close FL if it's running (it holds the DLL open and blocks the overwrite).
$fl = Get-Process -Name FL64, FL -ErrorAction SilentlyContinue
if ($fl) {
    Write-Host "Closing FL Studio ($($fl.Id -join ', '))..." -ForegroundColor Yellow
    $fl | Stop-Process -Force
    Start-Sleep -Milliseconds 800
}

# 2. Clean replace: delete the old bundle, then copy the new one.
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
Copy-Item -Recurse -Force $src $dst

# 3. Verify the actual binary matches (catches any silent skip).
$srcDll = Get-Item (Join-Path $src $dll)
$dstDll = Get-Item (Join-Path $dst $dll)
if ($srcDll.Length -eq $dstDll.Length) {
    Write-Host "Deployed OK: $($dstDll.Length) bytes, $($dstDll.LastWriteTime)" -ForegroundColor Green
} else {
    throw "SIZE MISMATCH -- copy did not land. src=$($srcDll.Length)  dst=$($dstDll.Length)"
}

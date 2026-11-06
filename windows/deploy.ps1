# Deploy clean winmm.dll to Navicat 18 install dir. ASCII only, full logging.
$ErrorActionPreference = "Continue"
$src   = Join-Path $PSScriptRoot "clean-proxy\winmm.dll"
$dst   = "C:\Program Files\Navicat\Navicat Premium 18\winmm.dll"
$dstV  = "C:\Program Files\Navicat\Navicat Premium 18\version.dll"
$dstE  = "C:\Program Files\Navicat\Navicat Premium 18\navicat.exe"
$log   = Join-Path $PSScriptRoot "deploy_log.txt"
$expect = "48E2ABD28D95094422DEC9C4BA0E5A54D7F7AF650D7A3F8974E74C22E3CF1D08"

function Log($m) {
    $line = ("{0} {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $m)
    Add-Content -Path $log -Value $line
    Write-Output $line
}

Log "=== deploy start (elevated) ==="

try {
    Log "step1: stop Navicat"
    Get-Process -Name navicat -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2

    Log "step2: check source"
    if (-not (Test-Path -LiteralPath $src)) { Log "FAIL: source missing $src"; exit 2 }
    $srcSize = (Get-Item -LiteralPath $src).Length
    Log ("source ok, size=" + $srcSize)

    Log "step3: remove leftover version.dll if any"
    if (Test-Path -LiteralPath $dstV) {
        Remove-Item -LiteralPath $dstV -Force
        Log "removed version.dll"
    } else { Log "no version.dll present" }

    Log "step4: backup existing winmm.dll"
    if (Test-Path -LiteralPath $dst) {
        Copy-Item -LiteralPath $dst -Destination ($dst + ".orig.bak") -Force
        Log "backed up to winmm.dll.orig.bak, old size=" + (Get-Item -LiteralPath $dst).Length
    } else { Log "no existing winmm.dll to back up" }

    Log "step5: deploy new winmm.dll"
    Copy-Item -LiteralPath $src -Destination $dst -Force
    if (-not (Test-Path -LiteralPath $dst)) { Log "FAIL: copy failed"; exit 3 }

    $h = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
    Log ("deployed size=" + (Get-Item -LiteralPath $dst).Length + " sha256=" + $h)
    if ($h -eq $expect) { Log "HASH MATCH expected" } else { Log "WARN: hash differs from expected" }

    Log "step6: start Navicat"
    Start-Process -FilePath $dstE
    Log "=== DEPLOY SUCCESS ==="
    exit 0
}
catch {
    Log ("EXCEPTION: " + $_.Exception.Message)
    exit 9
}

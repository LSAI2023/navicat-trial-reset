# Deploy auto-reset winmm.dll to Navicat 18 install dir. ASCII only, full logging.
$ErrorActionPreference = "Continue"
$src = Join-Path $PSScriptRoot "winmm.dll"
$dst = "C:\Program Files\Navicat\Navicat Premium 18\winmm.dll"
$log = Join-Path $PSScriptRoot "deploy_log.txt"
$expect = "AC0115830B01E21FAC75E1A826BE4C5A7400BFA5BCD96509CBAEE7CED65AD9C2"
function Log($m) { Add-Content -Path $log -Value (("{0} {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $m)) }
try {
    Log "=== deploy auto-reset proxy start ==="
    Get-Process -Name navicat -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    if (-not (Test-Path -LiteralPath $src)) { Log "FAIL: source missing"; exit 2 }
    if (Test-Path -LiteralPath $dst) {
        Copy-Item -LiteralPath $dst -Destination ($dst + ".prev.bak") -Force
        Log ("backed up previous dll size=" + (Get-Item -LiteralPath ($dst + ".prev.bak")).Length)
    }
    Copy-Item -LiteralPath $src -Destination $dst -Force
    $h = (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash
    Log ("deployed size=" + (Get-Item -LiteralPath $dst).Length + " sha256=" + $h)
    if ($h -eq $expect) { Log "HASH MATCH - DEPLOY_AUTORESET SUCCESS" } else { Log "HASH MISMATCH - FAIL" }
    exit 0
} catch { Log ("EXCEPTION: " + $_.Exception.Message); exit 9 }

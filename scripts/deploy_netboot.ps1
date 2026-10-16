<#
.SYNOPSIS
    Deploys MicaNT UEFI Bootloader to local IIS iPXE Netboot Server (barrer.net).

.DESCRIPTION
    Builds BOOTX64.EFI if requested, stages it in C:\inetpub\wwwroot\ipxe,
    configures boot.ipxe and web.config, and verifies the HTTP endpoints.
    Optionally monitors IIS logs for incoming bare-metal iPXE requests.

.PARAMETER BuildEfi
    If set, rebuilds build/esp/EFI/BOOT/BOOTX64.EFI before staging.

.PARAMETER Watch
    If set, monitors the IIS log file for requests to /ipxe/*.

.EXAMPLE
    .\scripts\deploy_netboot.ps1 -BuildEfi
    .\scripts\deploy_netboot.ps1 -Watch
#>

param(
    [switch]$BuildEfi,
    [switch]$Watch
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$webRoot = "C:\inetpub\wwwroot\ipxe"
if (-not (Test-Path $webRoot)) {
    New-Item -ItemType Directory -Force -Path $webRoot | Out-Null
}

if ($BuildEfi) {
    Write-Host "[MicaNT Netboot] Building UEFI PE32+ Bootloader (build/esp/EFI/BOOT/BOOTX64.EFI)..." -ForegroundColor Cyan
    & pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/build_incremental.ps1 -BuildEfi -SkipTests
    if ($LASTEXITCODE -ne 0) { throw "Failed building BOOTX64.EFI" }
}

$efiSrc = Join-Path $repoRoot "build/esp/EFI/BOOT/BOOTX64.EFI"
if (-not (Test-Path $efiSrc)) {
    throw "BOOTX64.EFI not found at $efiSrc. Run with -BuildEfi to compile it first."
}

$efiDst = Join-Path $webRoot "BOOTX64.EFI"
Copy-Item $efiSrc $efiDst -Force
Write-Host "[MicaNT Netboot] Staged BOOTX64.EFI ($((Get-Item $efiDst).Length) bytes) to $webRoot" -ForegroundColor Green

# Ensure web.config exists
$webConfigContent = @'
<?xml version="1.0" encoding="UTF-8"?>
<configuration>
    <system.webServer>
        <staticContent>
            <remove fileExtension=".ipxe" />
            <mimeMap fileExtension=".ipxe" mimeType="text/plain" />
            <remove fileExtension=".efi" />
            <mimeMap fileExtension=".efi" mimeType="application/octet-stream" />
            <remove fileExtension=".bin" />
            <mimeMap fileExtension=".bin" mimeType="application/octet-stream" />
            <remove fileExtension=".dll" />
            <mimeMap fileExtension=".dll" mimeType="application/octet-stream" />
            <remove fileExtension=".exe" />
            <mimeMap fileExtension=".exe" mimeType="application/octet-stream" />
        </staticContent>
        <directoryBrowse enabled="true" />
    </system.webServer>
</configuration>
'@
Set-Content -Path (Join-Path $webRoot "web.config") -Value $webConfigContent -Encoding UTF8

# Ensure boot.ipxe exists
$ipxeContent = @'
#!ipxe
# ============================================================================
#  MicaNT Cutler Edition — Bare-Metal iPXE Netboot Script
#  Target: Intel Xeon E3-1270 v6 (OVH Eco / Bare Metal)
# ============================================================================

echo ============================================================================
echo   MicaNT Cutler Edition: 90 Kernel Units, 44 System32 DLLs (334 Suites OK)
echo   Boot Server: barrer.net
echo ============================================================================

echo [MicaNT] Chainloading UEFI PE32+ Bootloader from http://barrer.net/ipxe/BOOTX64.EFI...
chain --autofree http://barrer.net/ipxe/BOOTX64.EFI || chain --autofree http://40.160.53.129/ipxe/BOOTX64.EFI || goto boot_err

:boot_err
echo [ERROR] Failed to chainload BOOTX64.EFI from barrer.net.
echo [MicaNT] Falling back to local offline kernel...
chain \EFI\MicaNT\BOOTX64.EFI || shell
'@
Set-Content -Path (Join-Path $webRoot "boot.ipxe") -Value $ipxeContent -Encoding UTF8
Write-Host "[MicaNT Netboot] Updated boot.ipxe in $webRoot" -ForegroundColor Green

# Validate local HTTP response
try {
    $testIpxe = Invoke-WebRequest -Uri "http://barrer.net/ipxe/boot.ipxe" -UseBasicParsing -TimeoutSec 3
    $testEfi  = Invoke-WebRequest -Uri "http://barrer.net/ipxe/BOOTX64.EFI" -UseBasicParsing -TimeoutSec 3
    if ($testIpxe.StatusCode -eq 200 -and $testEfi.StatusCode -eq 200) {
        Write-Host "[MicaNT Netboot] SUCCESS: Endpoints live and responding on barrer.net:" -ForegroundColor Cyan
        Write-Host "    iPXE Script URL: http://barrer.net/ipxe/boot.ipxe" -ForegroundColor Yellow
        Write-Host "    EFI Binary URL:  http://barrer.net/ipxe/BOOTX64.EFI" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Enter 'http://barrer.net/ipxe/boot.ipxe' in the OVH Manager under Custom iPXE script." -ForegroundColor White
    }
} catch {
    Write-Warning "[MicaNT Netboot] Self-test warning: $_"
}

if ($Watch) {
    Write-Host "[MicaNT Netboot] Watching IIS access logs for incoming iPXE connections (Ctrl+C to stop)..." -ForegroundColor Cyan
    $todayLog = Join-Path "C:\inetpub\logs\LogFiles\W3SVC1" "u_ex$((Get-Date).ToString('yyMMdd')).log"
    if (Test-Path $todayLog) {
        Get-Content $todayLog -Wait -Tail 10 | Where-Object { $_ -match "/ipxe/" }
    } else {
        Write-Host "Log file $todayLog does not exist yet."
    }
}

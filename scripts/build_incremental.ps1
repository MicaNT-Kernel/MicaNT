#!/usr/bin/env pwsh
<#
.SYNOPSIS
    CPU-Gentle Incremental Build Script for MicaNT (Ring 0 + Ring 1 + Ring 2 + Ring 3).
.DESCRIPTION
    1. Sets process priority to BelowNormal so compilation never pegs the host CPU or starves UI threads.
    2. Incrementally compiles only modified kernel/*.cpp units into build/clang_obj/*.o sequentially (max ~8% CPU on 12-thread Xeon).
    3. Links bin/micant_kernel.exe, bin/micant_tests.exe, and build/esp/EFI/BOOT/BOOTX64.EFI as requested.
#>

param(
    [switch]$Force,
    [switch]$SkipTests,
    [switch]$BuildEfi
)

$ErrorActionPreference = "Continue"
[System.Diagnostics.Process]::GetCurrentProcess().PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot

try {
    $objDir = Join-Path $repoRoot "build\clang_obj"
    $binDir = Join-Path $repoRoot "bin"
    New-Item -ItemType Directory -Force -Path $objDir | Out-Null
    New-Item -ItemType Directory -Force -Path $binDir | Out-Null

    $units = @(
        @{ Src = "kernel/ke/dispatcher.cpp";          Obj = "ke_dispatcher.o" },
        @{ Src = "kernel/ke/wait.cpp";                Obj = "ke_wait.o" },
        @{ Src = "kernel/hal/halinit.cpp";            Obj = "hal_halinit.o" },
        @{ Src = "kernel/mm/virtual.cpp";             Obj = "mm_virtual.o" },
        @{ Src = "kernel/ob/obref.cpp";               Obj = "ob_obref.o" },
        @{ Src = "kernel/ps/psmgr.cpp";               Obj = "ps_psmgr.o" },
        @{ Src = "kernel/io/iomgr.cpp";               Obj = "io_iomgr.o" },
        @{ Src = "kernel/se/ntsec.cpp";               Obj = "se_ntsec.o" },
        @{ Src = "kernel/cm/ntreg.cpp";               Obj = "cm_ntreg.o" },
        @{ Src = "kernel/lpc/ntlpc.cpp";              Obj = "lpc_ntlpc.o" },
        @{ Src = "kernel/ex/exmgr.cpp";               Obj = "ex_exmgr.o" },
        @{ Src = "kernel/cc/ccmgr.cpp";               Obj = "cc_ccmgr.o" },
        @{ Src = "kernel/fsrtl/fsrtlmgr.cpp";         Obj = "fsrtl_fsrtlmgr.o" },
        @{ Src = "kernel/pnp/pnpmgr.cpp";             Obj = "pnp_pnpmgr.o" },
        @{ Src = "kernel/po/pomgr.cpp";               Obj = "po_pomgr.o" },
        @{ Src = "kernel/wmi/wmimgr.cpp";             Obj = "wmi_wmimgr.o" },
        @{ Src = "kernel/dbgk/dbgkobj.cpp";           Obj = "dbgk_dbgkobj.o" },
        @{ Src = "kernel/tm/ktmmgr.cpp";              Obj = "tm_ktmmgr.o" },
        @{ Src = "kernel/verifier/vfmgr.cpp";         Obj = "verifier_vfmgr.o" },
        @{ Src = "kernel/arb/arbmgr.cpp";             Obj = "arb_arbmgr.o" },
        @{ Src = "kernel/io/pnpboot.cpp";             Obj = "io_pnpboot.o" },
        @{ Src = "kernel/io/pnpresilience.cpp";       Obj = "io_pnpresilience.o" },
        @{ Src = "kernel/po/pofx_whea.cpp";           Obj = "po_pofx_whea.o" },
        @{ Src = "kernel/io/umdf_msix.cpp";           Obj = "io_umdf_msix.o" },
        @{ Src = "kernel/ke/zerobsod.cpp";            Obj = "ke_zerobsod.o" },
        @{ Src = "kernel/io/livehotswap.cpp";         Obj = "io_livehotswap.o" },
        @{ Src = "kernel/ex/sysinfo_taskmgr.cpp";     Obj = "ex_sysinfo_taskmgr.o" },
        @{ Src = "kernel/ps/quotagov.cpp";            Obj = "ps_quotagov.o" },
        @{ Src = "kernel/fsrtl/mftzipui.cpp";         Obj = "fsrtl_mftzipui.o" },
        @{ Src = "kernel/se/srm_ci.cpp";              Obj = "se_srm_ci.o" },
        @{ Src = "kernel/io/afd_wsk.cpp";             Obj = "io_afd_wsk.o" },
        @{ Src = "kernel/ke/kuser_seal.cpp";          Obj = "ke_kuser_seal.o" },
        @{ Src = "kernel/fsrtl/ring1_vfs.cpp";        Obj = "fsrtl_ring1_vfs.o" },
        @{ Src = "kernel/io/ring1_dxgk.cpp";          Obj = "io_ring1_dxgk.o" },
        @{ Src = "kernel/io/ring1_audio.cpp";         Obj = "io_ring1_audio.o" },
        @{ Src = "kernel/ps/ring1_smss.cpp";          Obj = "ps_ring1_smss.o" },
        @{ Src = "kernel/io/ring2_wudf.cpp";          Obj = "io_ring2_wudf.o" },
        @{ Src = "kernel/se/ring2_lsa.cpp";           Obj = "se_ring2_lsa.o" },
        @{ Src = "kernel/ps/ring2_scm.cpp";           Obj = "ps_ring2_scm.o" },
        @{ Src = "kernel/ps/ring3_win32_base.cpp";       Obj = "ps_ring3_win32_base.o" },
        @{ Src = "kernel/ps/ring3_desktop_shell.cpp";    Obj = "ps_ring3_desktop_shell.o" },
        @{ Src = "kernel/ps/ring3_real_apps.cpp";        Obj = "ps_ring3_real_apps.o" },
        @{ Src = "kernel/io/ring3_prismx_3d12.cpp";                Obj = "io_ring3_prismx_3d12.o" },
        @{ Src = "kernel/io/ring3_directstorage_d2d.cpp";          Obj = "io_ring3_directstorage_d2d.o" },
        @{ Src = "kernel/io/ring3_prismaudio_mf.cpp";              Obj = "io_ring3_prismaudio_mf.o" },
        @{ Src = "kernel/io/ring3_winsock_winhttp_tls.cpp";        Obj = "io_ring3_winsock_winhttp_tls.o" },
        @{ Src = "kernel/lpc/ring3_com_rpc_shell.cpp";             Obj = "lpc_ring3_com_rpc_shell.o" },
        @{ Src = "kernel/se/ring3_bcrypt_crypt32_wintrust.cpp";    Obj = "se_ring3_bcrypt_crypt32_wintrust.o" },
        @{ Src = "kernel/io/ring3_fiber_net_apps.cpp";             Obj = "io_ring3_fiber_net_apps.o" },
        @{ Src = "kernel/ps/ring3_workstation_apps.cpp";           Obj = "ps_ring3_workstation_apps.o" },
        @{ Src = "kernel/ps/ring3_wsl2_hyperv_closure.cpp";        Obj = "ps_ring3_wsl2_hyperv_closure.o" },
        @{ Src = "kernel/io/prismx_directx_headers_kmt.cpp";       Obj = "io_prismx_directx_headers_kmt.o" },
        @{ Src = "kernel/io/prismx_dxmath_dxtex_dxmesh.cpp";       Obj = "io_prismx_dxmath_dxtex_dxmesh.o" },
        @{ Src = "kernel/io/prismx_d3d12_translation_on12.cpp";    Obj = "io_prismx_d3d12_translation_on12.o" },
        @{ Src = "kernel/io/prismx_dxc_sm68_pix.cpp";              Obj = "io_prismx_dxc_sm68_pix.o" },
        @{ Src = "kernel/io/prismx_directstorage_gdeflate.cpp";    Obj = "io_prismx_directstorage_gdeflate.o" },
        @{ Src = "kernel/io/prismx_directml_superres.cpp";         Obj = "io_prismx_directml_superres.o" },
        @{ Src = "kernel/io/prismx_enhanced_barriers_workgraphs.cpp"; Obj = "io_prismx_enhanced_barriers_workgraphs.o" },
        @{ Src = "kernel/io/prismx_vulkanon12_video_av1.cpp";         Obj = "io_prismx_vulkanon12_video_av1.o" },
        @{ Src = "kernel/io/prismx_dred_shadercache_zerotdr.cpp";     Obj = "io_prismx_dred_shadercache_zerotdr.o" }
    )

    $staleUnits = @()
    foreach ($u in $units) {
        $srcPath = Join-Path $repoRoot $u.Src
        $objPath = Join-Path $objDir $u.Obj
        if ($Force -or -not (Test-Path $objPath)) {
            $staleUnits += $u
        } else {
            $srcTime = (Get-Item $srcPath).LastWriteTimeUtc
            $objTime = (Get-Item $objPath).LastWriteTimeUtc
            if ($srcTime -gt $objTime) {
                $staleUnits += $u
            }
        }
    }

    Write-Host "[MicaNT Build] Incremental check: $($staleUnits.Count) / $($units.Count) kernel units need compilation (Single-Thread Sequential, Priority=BelowNormal)."
    foreach ($u in $staleUnits) {
        $src = $u.Src
        $obj = Join-Path "build/clang_obj" $u.Obj
        & clang++ -std=c++23 -O2 -Iinclude -Wno-unused-parameter -c $src -o $obj
        if ($LASTEXITCODE -ne 0) {
            throw "Failed compiling $src"
        }
    }

    $allObjs = $units | ForEach-Object { Join-Path "build/clang_obj" $_.Obj }

    Write-Host "[MicaNT Build] Linking bin/micant_kernel.exe..."
    & clang++ -std=c++23 -O2 -Iinclude kernel/init/kiinit.cpp @allObjs -o bin/micant_kernel.exe
    if ($LASTEXITCODE -ne 0) { throw "Failed linking bin/micant_kernel.exe" }

    if (-not $SkipTests) {
        Write-Host "[MicaNT Build] Linking bin/micant_tests.exe..."
        & clang++ -std=c++23 -O2 -Iinclude -I. test/test_runner.cpp @allObjs -o bin/micant_tests.exe
        if ($LASTEXITCODE -ne 0) { throw "Failed linking bin/micant_tests.exe" }
    }

    if ($BuildEfi) {
        Write-Host "[MicaNT Build] Building UEFI PE32+ Bootloader (build/esp/EFI/BOOT/BOOTX64.EFI)..."
        & clang++ -target x86_64-w64-mingw32 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -mno-red-zone -O2 -Iinclude "-Wl,--subsystem,efi_application" "-Wl,-e,EfiMain" -nostdlib boot/bootx64.cpp -o build/esp/EFI/BOOT/BOOTX64.EFI
        if ($LASTEXITCODE -ne 0) { throw "Failed building BOOTX64.EFI" }
    }

    Write-Host "[MicaNT Build] SUCCESS: Incremental build complete with BelowNormal CPU priority."
} finally {
    Pop-Location
}

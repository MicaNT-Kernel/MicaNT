# MicaNT Modular Sysroot Builder
# Assembles the authentic freestanding Windows NT operating system directory tree.

$ErrorActionPreference = "Stop"
$RepoRoot = "C:\Users\admin\source\MicaNT"
$Sysroot = "$RepoRoot\build\sysroot"
$EfiDir = "$Sysroot\EFI\BOOT"
$System32 = "$Sysroot\Windows\System32"
$DriversDir = "$System32\drivers"
$ConfigDir = "$System32\config"
$EtcDir = "$DriversDir\etc"

Write-Host "========================================================================" -ForegroundColor Cyan
Write-Host "             Building Authentic MicaNT Kernel Sysroot                   " -ForegroundColor Cyan
Write-Host "========================================================================" -ForegroundColor Cyan

# 1. Ensure directory hierarchy
New-Item -ItemType Directory -Force -Path $EfiDir | Out-Null
New-Item -ItemType Directory -Force -Path $System32 | Out-Null
New-Item -ItemType Directory -Force -Path $DriversDir | Out-Null
New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
New-Item -ItemType Directory -Force -Path $EtcDir | Out-Null

# 2. Build BOOTX64.EFI (Freestanding UEFI Boot Application)
Write-Host "[1/6] Compiling BOOTX64.EFI (Freestanding UEFI Boot Application)..." -ForegroundColor Yellow
$efiSrc = "$RepoRoot\boot\bootx64.cpp"
$efiOut = "$EfiDir\BOOTX64.EFI"
& clang++ -target x86_64-w64-mingw32 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti `
    -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -O2 -I "$RepoRoot\include" `
    "-Wl,--subsystem,efi_application" "-Wl,-e,EfiMain" -nostdlib `
    $efiSrc -o $efiOut
$sz = (Get-Item $efiOut).Length
Write-Host "      BOOTX64.EFI ready: $sz bytes" -ForegroundColor Green

# 3. Build ntoskrnl.exe (Freestanding Ring 0 Native Microkernel)
Write-Host "[2/6] Compiling ntoskrnl.exe (Freestanding Native Ring 0 Kernel)..." -ForegroundColor Yellow
$krnlSrc = "$RepoRoot\kernel\init\ntoskrnl.cpp"
$krnlOut = "$System32\ntoskrnl.exe"
& clang++ -target x86_64-w64-mingw32 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti `
    -fno-threadsafe-statics -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -O2 -I "$RepoRoot\include" `
    "-Wl,--subsystem,native" "-Wl,-e,KiSystemStartup" -nostdlib `
    $krnlSrc -o $krnlOut
$sz = (Get-Item $krnlOut).Length
Write-Host "      ntoskrnl.exe ready: $sz bytes" -ForegroundColor Green

# 4. Build Known DLLs (PE32+ Native / Shared Libraries)
Write-Host "[3/6] Compiling Known DLLs into System32..." -ForegroundColor Yellow
$dllList = @(
    @{ src = "tools\dlls\ntdll.cpp";      name = "ntdll.dll" },
    @{ src = "tools\dlls\kernel32.cpp";   name = "kernel32.dll" },
    @{ src = "tools\dlls\kernelbase.cpp"; name = "kernelbase.dll" },
    @{ src = "tools\dlls\user32.cpp";     name = "user32.dll" },
    @{ src = "tools\dlls\gdi32.cpp";      name = "gdi32.dll" },
    @{ src = "tools\dlls\advapi32.cpp";   name = "advapi32.dll" },
    @{ src = "tools\dlls\comctl32.cpp";   name = "comctl32.dll" },
    @{ src = "tools\dlls\comdlg32.cpp";   name = "comdlg32.dll" },
    @{ src = "tools\dlls\shell32.cpp";    name = "shell32.dll" },
    @{ src = "tools\dlls\shlwapi.cpp";    name = "shlwapi.dll" },
    @{ src = "tools\dlls\ole32.cpp";      name = "ole32.dll" },
    @{ src = "tools\dlls\ucrtbase.cpp";   name = "ucrtbase.dll" }
)
foreach ($d in $dllList) {
    $srcPath = "$RepoRoot\$($d.src)"
    $outPath = "$System32\$($d.name)"
    if (Test-Path $srcPath) {
        & clang++ -target x86_64-w64-mingw32 -shared -nostdlib "-Wl,-e,DllMain" "-Wl,--subsystem,native" -O2 -I "$RepoRoot\include" $srcPath -o $outPath
        Write-Host "      Compiled $($d.name)" -ForegroundColor Gray
    }
}

# 5. Build Kernel Drivers (.sys)
Write-Host "[4/6] Compiling Kernel Drivers (.sys) into System32\drivers..." -ForegroundColor Yellow
$driverList = @(
    @{ src = "tools\drivers\driver_template.cpp"; name = "ahci.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "i210.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "tcpip.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "fltmgr.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "xhci.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "usbhub.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "extfs.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "null.sys" },
    @{ src = "tools\drivers\driver_template.cpp"; name = "beep.sys" },
    @{ src = "tools\drivers\ntfs_driver.cpp";     name = "ntfs.sys" },
    @{ src = "tools\drivers\exfat_driver.cpp";    name = "exfat.sys" },
    @{ src = "tools\drivers\cdfs_driver.cpp";     name = "cdfs.sys" },
    @{ src = "tools\drivers\udfs_driver.cpp";     name = "udfs.sys" },
    @{ src = "tools\drivers\refs_driver.cpp";     name = "refs.sys" },
    @{ src = "tools\drivers\zfs_driver.cpp";      name = "zfs.sys" }
)
foreach ($drv in $driverList) {
    $srcPath = "$RepoRoot\$($drv.src)"
    $outPath = "$DriversDir\$($drv.name)"
    if (Test-Path $srcPath) {
        & clang++ -target x86_64-w64-mingw32 -nostdlib "-Wl,-e,DriverEntry" "-Wl,--subsystem,native" -O2 -I "$RepoRoot\include" $srcPath -o $outPath
        Write-Host "      Compiled $($drv.name)" -ForegroundColor Gray
    }
}

# 6. Build Core System Services & Tools
Write-Host "[5/6] Compiling System Services and Shell Tools..." -ForegroundColor Yellow
$svcList = @(
    @{ src = "tools\smss_app.cpp";      name = "smss.exe" },
    @{ src = "tools\csrss_app.cpp";     name = "csrss.exe" },
    @{ src = "tools\winlogon_app.cpp";  name = "winlogon.exe" },
    @{ src = "tools\services_app.cpp";  name = "services.exe" },
    @{ src = "tools\lsass_app.cpp";     name = "lsass.exe" },
    @{ src = "tools\svchost_app.cpp";   name = "svchost.exe" },
    @{ src = "tools\conhost_app.cpp";   name = "conhost.exe" },
    @{ src = "tools\cmd_app.cpp";       name = "cmd.exe" },
    @{ src = "tools\ipconfig_app.cpp";  name = "ipconfig.exe" },
    @{ src = "tools\whoami_app.cpp";    name = "whoami.exe" },
    @{ src = "tools\tasklist_app.cpp";  name = "tasklist.exe" },
    @{ src = "tools\systeminfo_app.cpp";name = "systeminfo.exe" },
    @{ src = "tools\ping_app.cpp";      name = "ping.exe" },
    @{ src = "tools\netstat_app.cpp";   name = "netstat.exe" },
    @{ src = "tools\chkdsk_app.cpp";    name = "chkdsk.exe" },
    @{ src = "tools\diskpart_app.cpp";  name = "diskpart.exe" },
    @{ src = "tools\reg_app.cpp";       name = "reg.exe" },
    @{ src = "tools\taskkill_app.cpp";  name = "taskkill.exe" },
    @{ src = "tools\shutdown_app.cpp";  name = "shutdown.exe" },
    @{ src = "tools\explorer_app.cpp";  name = "explorer.exe" },
    @{ src = "tools\taskmgr_app.cpp";   name = "taskmgr.exe" },
    @{ src = "tools\notepad_app.cpp";   name = "notepad.exe" }
)
foreach ($s in $svcList) {
    $srcPath = "$RepoRoot\$($s.src)"
    $outPath = "$System32\$($s.name)"
    if (Test-Path $srcPath) {
        & clang++ -target x86_64-w64-mingw32 -std=c++23 -ffreestanding -fno-exceptions -fno-rtti `
            -fno-stack-protector -mno-red-zone -mno-stack-arg-probe -O2 -I "$RepoRoot\include" -I "$RepoRoot\tools" `
            "-Wl,--subsystem,console" "-Wl,-e,_start" -nostdlib `
            $srcPath -o $outPath
        Write-Host "      Compiled $($s.name)" -ForegroundColor Gray
    }
}

# 7. Generate Registry Hives & Network Configs
Write-Host "[6/6] Populating Registry Hives & Network Configs..." -ForegroundColor Yellow
& python "$RepoRoot\tools\config\gen_hives.py" | Out-Null
$hives = @("SYSTEM", "SOFTWARE", "SAM", "SECURITY", "DEFAULT")
foreach ($h in $hives) {
    Copy-Item "$RepoRoot\tools\config\$h" "$ConfigDir\$h" -Force
    Write-Host "      Installed hive: $h" -ForegroundColor Gray
}

$netFiles = @("hosts", "services", "networks", "protocol")
foreach ($nf in $netFiles) {
    $src = "$RepoRoot\tools\etc\$nf"
    if (Test-Path $src) {
        Copy-Item $src "$EtcDir\$nf" -Force
        Write-Host "      Installed network config: $nf" -ForegroundColor Gray
    }
}

Write-Host "========================================================================" -ForegroundColor Green
Write-Host "       MicaNT Modular Sysroot Assembly Complete!                       " -ForegroundColor Green
Write-Host "========================================================================" -ForegroundColor Green

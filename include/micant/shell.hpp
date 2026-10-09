#pragma once

/**
 * @file shell.hpp
 * @brief MicaNT Command Prompt & Shell Engine (cmd.exe / msh.exe).
 *
 * Provides a clean-room command processor, virtual filesystem browser,
 * environment variable manipulator, and native PE binary launcher.
 */

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <atomic>
#include <cmath>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "conhost.hpp"
#include "fs.hpp"
#include "pe.hpp"
#include "csrss.hpp"
#include "hal.hpp"
#include "ex.hpp"
#include "mm.hpp"
#include "msvcrt.hpp"
#include "advapi32.hpp"
#include "user32.hpp"
#include "ws2_32.hpp"
#include "ndis.hpp"
#include "tcpip.hpp"
#include "iphlpapi.hpp"
#include "prismx.hpp"
#include "prism3d.hpp"
#include "prism3d12.hpp"
#include "prism_shader_vm.hpp"
#include "dxgkrnl.hpp"
#include "vulkan.hpp"
#include "prism_viewer.hpp"
#include "d3d9.hpp"
#include "gdi32.hpp"
#include "ole32.hpp"
#include "shell32.hpp"
#include "comctl32.hpp"
#include "cmd.hpp"
#include "winmm.hpp"
#include "dsound.hpp"
#include "version.hpp"
#include "opengl.hpp"
#include "wininet.hpp"
#include "urlmon.hpp"
#include "cipherksp.hpp"
#include "crypt32.hpp"
#include "sspi.hpp"
#include "rpcrt4.hpp"
#include "oleaut32.hpp"
#include "setupapi.hpp"
#include "wevtapi.hpp"
#include "wbem.hpp"
#include "taskschd.hpp"
#include "bits.hpp"
#include "vss.hpp"
#include "wer.hpp"
#include "dwmapi.hpp"
#include "wasapi.hpp"
#include "cbs.hpp"
#include "wdi.hpp"
#include "pdh.hpp"
#include "etw.hpp"
#include "acl.hpp"
#include "netapi32.hpp"
#include "ldap.hpp"
#include "termsrv.hpp"
#include "winspool.hpp"
#include "mci.hpp"
#include "winscard.hpp"
#include "nla.hpp"
#include "wns.hpp"
#include "location.hpp"
#include "wpd.hpp"
#include "sensors.hpp"
#include "winbio.hpp"
#include "bluetooth.hpp"
#include "cardmod.hpp"
#include "posix.hpp"
#include "whp.hpp"
#include "dwrite.hpp"
#include "mfplat.hpp"
#include "dshow.hpp"
#include "wmp.hpp"
#include "gdiplus.hpp"
#include "d2d1.hpp"
#include "mfsession.hpp"
#include "evr.hpp"
#include "dxva2.hpp"
#include "d3d11va.hpp"
#include "d3d12video.hpp"
#include "mfreadwrite.hpp"
#include "mfcaptureengine.hpp"
#include "d3d12raytracing.hpp"
#include "directstorage.hpp"
#include "dxcore.hpp"
#include "directml.hpp"
#include "dcomp.hpp"
#include "uicomposition.hpp"
#include "wcs.hpp"
#include "pointer.hpp"
#include "appmodel.hpp"
#include "d2d1_3.hpp"
#include "tsf.hpp"
#include "spellcheck.hpp"
#include "sapi.hpp"
#include "ocr.hpp"
#include "winml.hpp"
#include "webauthn.hpp"
#include "wlanapi.hpp"
#include "virtdisk.hpp"
#include "fveapi.hpp"
#include "fwpuclnt.hpp"
#include "wintrust.hpp"
#include "ci.hpp"
#include "feclient.hpp"
#include "wscapi.hpp"
#include "amsi.hpp"
#include "mpengine.hpp"
#include "exploit_guard.hpp"
#include "credguard.hpp"
#include "ppl.hpp"
#include "sysguard.hpp"
#include "vbs_hvci.hpp"
#include "dma_guard.hpp"
#include "wsl_lxss.hpp"
#include "sandbox.hpp"
#include "whp.hpp"
#include "winget.hpp"
#include "wdf.hpp"
#include "conpty.hpp"
#include "usb.hpp"
#include "pci.hpp"
#include "nvme.hpp"
#include "acpi.hpp"
#include "hdaudio.hpp"
#include "wddm.hpp"
#include "bthport.hpp"
#include "wdiwifi.hpp"
#include "usb4.hpp"
#include "npu.hpp"
#include "cxl.hpp"
#include "ucsi.hpp"
#include "bypassio.hpp"
#include "pmem.hpp"
#include "rdma.hpp"
#include "pluton.hpp"
#include "hfi.hpp"
#include "cet.hpp"
#include "qat.hpp"
#include "tee.hpp"
#include "dsa.hpp"
#include "amx.hpp"
#include "sriov.hpp"
#include "iommu.hpp"
#include "uefi_rt.hpp"
#include "modern_standby.hpp"
#include "wsa.hpp"
#include "touchpad.hpp"
#include "ink.hpp"
#include "spatial_audio.hpp"
#include "hpd.hpp"
#include "cameracx.hpp"
#include "vrr.hpp"
#include "sensorscx.hpp"
#include "mbbcx.hpp"
#include "pmp.hpp"
#include "vmbus.hpp"
#include "vpci.hpp"
#include "vsm.hpp"
#include "hotpatch.hpp"
#include "hyperv.hpp"
#include "refs.hpp"
#include "csvfs.hpp"
#include "wcifs.hpp"
#include "s2d.hpp"
#include "branchcache.hpp"
#include "storage_replica.hpp"
#include "clustering.hpp"
#include "vmms.hpp"
#include "activedirectory.hpp"
#include "grouppolicy.hpp"
#include "remotedesktop.hpp"
#include "nps.hpp"
#include "wsrm.hpp"
#include "wds.hpp"
#include "certsrv.hpp"
#include "dns_server.hpp"
#include "dhcp_server.hpp"
#include "iis_server.hpp"
#include "wsus_server.hpp"
#include "winrm_server.hpp"
#include "ssh.hpp"

namespace micant::shell {

namespace host_thread {
    extern "C" void*    __stdcall CreateThread(void* lpThreadAttributes, size_t dwStackSize, uint32_t (__stdcall *lpStartAddress)(void*), void* lpParameter, uint32_t dwCreationFlags, uint32_t* lpThreadId);
    extern "C" uint32_t __stdcall WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds);
    extern "C" int      __stdcall GetExitCodeThread(void* hThread, uint32_t* lpExitCode);
    extern "C" int      __stdcall CloseHandle(void* hObject);
    extern "C" void     __stdcall ExitThread(uint32_t dwExitCode);
}

inline std::atomic<uint32_t> s_LastBinaryExitCode{0};
inline std::atomic<bool> s_LastBinaryExited{false};

[[noreturn]] inline void ShellBinaryExitProcess(uint32_t code) noexcept {
    s_LastBinaryExitCode.store(code);
    s_LastBinaryExited.store(true);
    host_thread::ExitThread(code);
    for (;;) {}
}

[[noreturn]] inline int32_t ShellBinaryTerminateProcess(void* /*hProcess*/, uint32_t code) noexcept {
    s_LastBinaryExitCode.store(code);
    s_LastBinaryExited.store(true);
    host_thread::ExitThread(code);
    for (;;) {}
}

namespace host_mem {
    extern "C" void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize, uint32_t flAllocationType, uint32_t flProtect);
    extern "C" int   __stdcall VirtualFree(void* lpAddress, size_t dwSize, uint32_t dwFreeType);
}

class CommandShell {
public:
    CommandShell() {
        win32::InitializeWin32SubsystemExports();
        msvcrt::InitializeMsvcrtSubsystemExports();
        advapi32::InitializeAdvapi32SubsystemExports();
        user32::InitializeUser32SubsystemExports();
        ws2_32::InitializeWs2_32SubsystemExports();
        iphlpapi::InitializeIpHlpApiSubsystemExports();
        d3d9::InitializeD3D9SubsystemExports();
        gdi32::InitializeGdi32SubsystemExports();
        ole32::InitializeOle32SubsystemExports();
        shell32::InitializeShell32SubsystemExports();
        comctl32::InitializeComCtl32SubsystemExports();
        opengl::InitializeOpenglSubsystemExports();
        wininet::InitializeWinINetSubsystemExports();
        urlmon::InitializeUrlMonSubsystemExports();
        crypto::InitializeBCryptSubsystemExports();
        crypt32::InitializeCrypt32SubsystemExports();
        sspi::InitializeSspiSubsystemExports();
        rpc::InitializeRpcSubsystemExports();
        oleaut32::InitializeOleAut32SubsystemExports();
        setupapi::InitializeSetupApiSubsystemExports();
        wevtapi::InitializeWevtApiSubsystemExports();
        wbem::InitializeWbemSubsystemExports();
        cbs::InitializeCbsSubsystemExports();
        mci::InitializeMciSubsystemExports();
        scard::InitializeWinSCardSubsystemExports();
        nla::InitializeNlaSubsystemExports();
        tcpip::NetworkStack::get().initialize();
        d3d12video::InitializeD3D12VideoExports();
        mfreadwrite::InitializeMFReadWriteExports();
        mfcapture::InitializeMFCaptureEngineExports();
        dxr::InitializeDXRExports();
        wsc::InitializeWscSubsystemExports();
        amsi::InitializeAmsiSubsystemExports();
        defender::InitializeMpEngineSubsystemExports();
        exploit_guard::InitializeExploitGuardSubsystemExports();
        credguard::InitializeCredGuardSubsystemExports();
        ppl::InitializePplSubsystemExports();
        sysguard::InitializeSysGuardSubsystemExports();
        vbs_hvci::InitializeVbsHvciSubsystemExports();
        dma_guard::InitializeDmaGuardSubsystemExports();
        wsl_lxss::InitializeWslSubsystemExports();
        sandbox::InitializeSandboxSubsystemExports();
        whp::InitializeWhpSubsystemExports();
        winget::InitializeWinGetSubsystemExports();

        // Establish default interactive logon session (admin) if not already active
        if (winlogon::WinlogonManager::get().getState() == winlogon::LogonState::LoggedOff) {
            winlogon::WinlogonManager::get().initiateLogon(L"admin", L"mica");
        }
    }

    void printBanner(std::ostream& out = std::cout) {
        out << "\n";
        out << "MicaNT [Version 10.0.26100.1]\n";
        out << "(c) Project MICA. Dave Cutler Clean-Room Architecture. Zero Telemetry.\n";
        out << "Type 'help' or '?' for available shell commands.\n\n";
    }

    std::string getCurrentDirectory() {
        wchar_t buf[260]{};
        win32::GetCurrentDirectoryW(260, buf);
        std::string s;
        for (int i = 0; buf[i] != L'\0'; ++i) s.push_back(static_cast<char>(buf[i] & 0x7F));
        return s;
    }

    std::string promptString() {
        return micant::cmd::CmdProcessor::get().getPromptString();
    }

    int execute(std::string_view commandLine, std::ostream& out = std::cout) {
        std::string line = trim(commandLine);
        if (line.empty()) return 0;

        bool hasCompound = (line.find('&') != std::string::npos ||
                            line.find('|') != std::string::npos ||
                            line.find('>') != std::string::npos ||
                            line.find('<') != std::string::npos);

        auto tokens = tokenize(line);
        if (tokens.empty()) return 0;

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (cmd == "exit" || cmd == "quit") {
            return -1; // Request shell exit
        }

        // SentinelScan (AMSI) In-Memory Script & Command Inspection
        if (cmd != "amsi" && cmd != "sentinelscan" && cmd != "sentinel" && cmd != "wsc" &&
            cmd != "security" && cmd != "securitycenter" && cmd != "mpcmdrun" && cmd != "defender" &&
            cmd != "guard" && cmd != "exploitguard" && cmd != "mitlib" &&
            cmd != "credguard" && cmd != "cred" && cmd != "lsaiso" &&
            cmd != "ppl" && cmd != "protectedprocess" && cmd != "elam" && cmd != "bootdriver" &&
            cmd != "sysguard" && cmd != "systemguard" && cmd != "measuredboot" && cmd != "tbs" &&
            cmd != "vbs" && cmd != "hvci" &&
            cmd != "dmaguard" && cmd != "dma" &&
            cmd != "wsl" && cmd != "bash" && cmd != "lxss" &&
            cmd != "sandbox" && cmd != "wsb" &&
            cmd != "whp" && cmd != "hyperv" &&
            cmd != "winget" && cmd != "appinstaller" &&
            cmd != "help" && cmd != "?") {
            std::wstring wline;
            wline.reserve(line.size());
            for (char c : line) wline.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));

            amsi::HAMSICONTEXT amsiCtx = nullptr;
            amsi::HAMSISESSION amsiSess = nullptr;
            if (SUCCEEDED(amsi::AmsiInitialize(L"MicaNTCommandShell", &amsiCtx))) {
                amsi::AmsiOpenSession(amsiCtx, &amsiSess);
                amsi::AMSI_RESULT amsiRes = amsi::AMSI_RESULT_CLEAN;
                amsi::AmsiScanString(amsiCtx, wline.c_str(), L"CommandPrompt.cmd", amsiSess, &amsiRes);
                amsi::AmsiCloseSession(amsiCtx, amsiSess);
                amsi::AmsiUninitialize(amsiCtx);

                if (amsi::AmsiResultIsMalware(amsiRes)) {
                    out << "[-] Blocked by Sentinel Security System (AMSI / SentinelScan): Malicious script pattern or threat detected.\n"
                        << "[-] Error: 0x800700DF (ERROR_VIRUS_INFECTED: The file contains a virus or potentially unwanted software).\n";
                    return 1;
                } else if (amsi::AmsiResultIsBlockedByAdmin(amsiRes)) {
                    out << "[-] Blocked by Sentinel Security System (AMSI): Execution denied by administrator security policy.\n"
                        << "[-] Error: 0x800704EC (ERROR_ACCESS_DISABLED_BY_POLICY).\n";
                    return 1;
                }
            }
        }

        // Check MicaNT executive test & diagnostic commands
        if (!hasCompound || cmd == "sapi" || cmd == "speech" || cmd == "voice" || cmd == "tts") {
            if (cmd == "mem") { cmdMem(out); return 0; }
            if (cmd == "systeminfo") { cmdSystemInfo(out); return 0; }
            if (cmd == "ping") { cmdPing(tokens, out); return 0; }
            if (cmd == "ipconfig") { cmdIpConfig(tokens, out); return 0; }
            if (cmd == "netstat") { cmdNetstat(tokens, out); return 0; }
            if (cmd == "net") { cmdNet(tokens, out); return 0; }
            if (cmd == "sc") { cmdSc(tokens, out); return 0; }
            if (cmd == "whoami") { cmdWhoami(tokens, out); return 0; }
            if (cmd == "prismx" || cmd == "gpu") { cmdPrismX(tokens, out); return 0; }
            if (cmd == "d3d9" || cmd == "dx9") { cmdD3D9(tokens, out); return 0; }
            if (cmd == "gdi") { cmdGDI(tokens, out); return 0; }
            if (cmd == "com" || cmd == "ole") { cmdCOM(tokens, out); return 0; }
            if (cmd == "shell32") { cmdShell32(tokens, out); return 0; }
            if (cmd == "comctl" || cmd == "commoncontrols") { cmdComCtl(tokens, out); return 0; }
            if (cmd == "view3d" || cmd == "viewer3d") { cmdView3D(tokens, out); return 0; }
            if (cmd == "vulkan" || cmd == "vkinfo" || cmd == "vkcube") { cmdVulkan(tokens, out); return 0; }
            if (cmd == "winmm" || cmd == "mmsys") { cmdWinMM(tokens, out); return 0; }
            if (cmd == "dsound" || cmd == "directsound") { cmdDirectSound(tokens, out); return 0; }
            if (cmd == "version" || cmd == "verinfo") { cmdVersion(tokens, out); return 0; }
            if (cmd == "opengl" || cmd == "gl" || cmd == "wgl") { cmdOpenGL(tokens, out); return 0; }
            if (cmd == "wininet" || cmd == "internet") { cmdWinINet(tokens, out); return 0; }
            if (cmd == "urlmon" || cmd == "curl" || cmd == "wget" || cmd == "download") { cmdUrlMon(tokens, out); return 0; }
            if (cmd == "bcrypt" || cmd == "cng") { cmdBCrypt(tokens, out); return 0; }
            if (cmd == "certmgr" || cmd == "cert") { cmdCertMgr(tokens, out); return 0; }
            if (cmd == "dpapi") { cmdDpapi(tokens, out); return 0; }
            if (cmd == "sspi") { cmdSspi(tokens, out); return 0; }
            if (cmd == "schannel") { cmdSchannel(tokens, out); return 0; }
            if (cmd == "rpc") { cmdRpc(tokens, out); return 0; }
            if (cmd == "uuidgen") { cmdUuidGen(tokens, out); return 0; }
            if (cmd == "oleaut" || cmd == "safearray" || cmd == "variant") { cmdOleAut(tokens, out); return 0; }
            if (cmd == "devmgmt" || cmd == "setupapi") { cmdDevMgmt(tokens, out); return 0; }
            if (cmd == "stg" || cmd == "storage" || cmd == "docfile") { cmdStorage(tokens, out); return 0; }
            if (cmd == "wevtutil" || cmd == "eventlog" || cmd == "eventviewer") { cmdWevtUtil(tokens, out); return 0; }
            if (cmd == "wmic" || cmd == "wbem") { cmdWmic(tokens, out); return 0; }
            if (cmd == "schtasks" || cmd == "taskschd") { cmdSchtasks(tokens, out); return 0; }
            if (cmd == "bitsadmin" || cmd == "bits") { cmdBitsAdmin(tokens, out); return 0; }
            if (cmd == "vssadmin" || cmd == "vss") { cmdVssAdmin(tokens, out); return 0; }
            if (cmd == "werfault" || cmd == "wer") { cmdWerFault(tokens, out); return 0; }
            if (cmd == "dwm" || cmd == "dwmapi") { cmdDwm(tokens, out); return 0; }
            if (cmd == "audiosrv" || cmd == "wasapi") { cmdAudioSrv(tokens, out); return 0; }
            if (cmd == "dism") { cmdDism(tokens, out); return 0; }
            if (cmd == "msdt" || cmd == "wdi") { cmdMsdt(tokens, out); return 0; }
            if (cmd == "perfmon") { cmdPerfMon(tokens, out); return 0; }
            if (cmd == "typeperf") { cmdTypePerf(tokens, out); return 0; }
            if (cmd == "logman") { cmdLogman(tokens, out); return 0; }
            if (cmd == "tracerpt") { cmdTraceRpt(tokens, out); return 0; }
            if (cmd == "icacls" || cmd == "cacls") { cmdIcacls(tokens, out); return 0; }
            if (cmd == "auditpol") { cmdAuditPol(tokens, out); return 0; }
            if (cmd == "dsquery") { cmdDsQuery(tokens, out); return 0; }
            if (cmd == "dsget") { cmdDsGet(tokens, out); return 0; }
            if (cmd == "qwinsta") { cmdQWinsta(tokens, out); return 0; }
            if (cmd == "rwinsta") { cmdRWinsta(tokens, out); return 0; }
            if (cmd == "mstsc") { cmdMstsc(tokens, out); return 0; }
            if (cmd == "prnmngr") { cmdPrnMngr(tokens, out); return 0; }
            if (cmd == "print") { cmdPrint(tokens, out); return 0; }
            if (cmd == "mci") { cmdMci(tokens, out); return 0; }
            if (cmd == "waveplay") { cmdWavePlay(tokens, out); return 0; }
            if (cmd == "scard" || cmd == "smartcard") { cmdSCard(tokens, out); return 0; }
            if (cmd == "nla" || cmd == "netprof") { cmdNla(tokens, out); return 0; }
            if (cmd == "notify" || cmd == "toast" || cmd == "wns") { cmdNotify(tokens, out); return 0; }
            if (cmd == "location" || cmd == "geo" || cmd == "gps") { cmdLocation(tokens, out); return 0; }
            if (cmd == "wpd" || cmd == "pdevice") { cmdWpd(tokens, out); return 0; }
            if (cmd == "sensor" || cmd == "sensors") { cmdSensor(tokens, out); return 0; }
            if (cmd == "winbio" || cmd == "bio" || cmd == "hello") { cmdWinBio(tokens, out); return 0; }
            if (cmd == "bluetooth" || cmd == "bth" || cmd == "bt") { cmdBluetooth(tokens, out); return 0; }
            if (cmd == "cardmod" || cmd == "scminidriver") { cmdCardMod(tokens, out); return 0; }
            if (cmd == "posix" || cmd == "psx" || cmd == "sua") { cmdPosix(tokens, out); return 0; }
            if (cmd == "whp" || cmd == "viridian") { cmdWhp(tokens, out); return 0; }
            if (cmd == "dwrite" || cmd == "uniscribe" || cmd == "typography") { cmdDWrite(tokens, out); return 0; }
            if (cmd == "mf" || cmd == "mediafoundation") { cmdMediaFoundation(tokens, out); return 0; }
            if (cmd == "dshow" || cmd == "filtergraph") { cmdDirectShow(tokens, out); return 0; }
            if (cmd == "wmp" || cmd == "mediaplayer" || cmd == "wmplayer") { cmdWMP(tokens, out); return 0; }
            if (cmd == "gdiplus" || cmd == "gdi+" || cmd == "wic" || cmd == "mspaint" || cmd == "paint") { cmdGdiPlus(tokens, out); return 0; }
            if (cmd == "d2d" || cmd == "d2d1" || cmd == "direct2d") { cmdDirect2D(tokens, out); return 0; }
            if (cmd == "mfsession" || cmd == "topology" || cmd == "mfpipeline") { cmdMFSession(tokens, out); return 0; }
            if (cmd == "evr" || cmd == "renderer") { cmdEVR(tokens, out); return 0; }
            if (cmd == "dxva" || cmd == "dxva2") { cmdDXVA2(tokens, out); return 0; }
            if (cmd == "d3d11va" || cmd == "d3d11video" || cmd == "d3d11v") { cmdD3D11VA(tokens, out); return 0; }
            if (cmd == "d3d12video" || cmd == "d3d12v") { cmdD3D12Video(tokens, out); return 0; }
            if (cmd == "mfreadwrite" || cmd == "sourcereader" || cmd == "sinkwriter") { cmdMFReadWrite(tokens, out); return 0; }
            if (cmd == "mfcapture" || cmd == "captureengine") { cmdMFCapture(tokens, out); return 0; }
            if (cmd == "dxr" || cmd == "raytracing" || cmd == "meshshader") { cmdDXR(tokens, out); return 0; }
            if (cmd == "dstorage" || cmd == "directstorage") { cmdDirectStorage(tokens, out); return 0; }
            if (cmd == "dml" || cmd == "directml" || cmd == "dxcore") { cmdDirectML(tokens, out); return 0; }
            if (cmd == "dcomp" || cmd == "directcomposition" || cmd == "compositor") { cmdDirectComposition(tokens, out); return 0; }
            if (cmd == "uicomp" || cmd == "composition" || cmd == "visuals") { cmdUIComposition(tokens, out); return 0; }
            if (cmd == "wcs" || cmd == "colorsystem" || cmd == "colormgr") { cmdColorSystem(tokens, out); return 0; }
            if (cmd == "pointer" || cmd == "touch" || cmd == "ink") { cmdPointer(tokens, out); return 0; }
            if (cmd == "appmodel" || cmd == "package" || cmd == "plm" || cmd == "appx") { cmdAppModel(tokens, out); return 0; }
            if (cmd == "d2d13" || cmd == "d2d3" || cmd == "typography" || cmd == "svg") { cmdD2D1_3(tokens, out); return 0; }
            if (cmd == "tsf" || cmd == "ime" || cmd == "textservices") { cmdTSF(tokens, out); return 0; }
            if (cmd == "spell" || cmd == "spellcheck" || cmd == "els" || cmd == "linguistic") { cmdSpellCheck(tokens, out); return 0; }
            if (cmd == "sapi" || cmd == "speech" || cmd == "voice" || cmd == "tts") { cmdSapi(tokens, out); return 0; }
            if (cmd == "ocr" || cmd == "vision") { cmdOcr(tokens, out); return 0; }
            if (cmd == "winml" || cmd == "ml" || cmd == "ai") { cmdWinML(tokens, out); return 0; }
            if (cmd == "webauthn" || cmd == "fido2" || cmd == "passkey") { cmdWebAuthn(tokens, out); return 0; }
            if (cmd == "wlan" || cmd == "wifi" || cmd == "wireless") { cmdWlan(tokens, out); return 0; }
            if (cmd == "vhd" || cmd == "virtdisk" || cmd == "vdisk") { cmdVirtDisk(tokens, out); return 0; }
            if (cmd == "manage-bde" || cmd == "bde" || cmd == "bitlocker") { cmdManageBde(tokens, out); return 0; }
            if (cmd == "netsh" || cmd == "advfirewall" || cmd == "firewall" || cmd == "wfp") { cmdFirewall(tokens, out); return 0; }
            if (cmd == "signtool" || cmd == "wintrust" || cmd == "sign") { cmdSignTool(tokens, out); return 0; }
            if (cmd == "wdac" || cmd == "ci") { cmdWdac(tokens, out); return 0; }
            if (cmd == "cipher" || cmd == "efs") { cmdCipher(tokens, out); return 0; }
            if (cmd == "sentinel" || cmd == "wsc" || cmd == "security" || cmd == "securitycenter") { cmdWsc(tokens, out); return 0; }
            if (cmd == "amsi" || cmd == "sentinelscan") { cmdAmsi(tokens, out); return 0; }
            if (cmd == "mpcmdrun" || cmd == "defender") { cmdMpCmdRun(tokens, out); return 0; }
            if (cmd == "guard" || cmd == "exploitguard" || cmd == "mitlib" || cmd == "mitigation") { cmdSentinelGuard(tokens, out); return 0; }
            if (cmd == "credguard" || cmd == "cred" || cmd == "lsaiso") { cmdCredGuard(tokens, out); return 0; }
            if (cmd == "ppl" || cmd == "protectedprocess") { cmdPpl(tokens, out); return 0; }
            if (cmd == "elam" || cmd == "bootdriver") { cmdElam(tokens, out); return 0; }
            if (cmd == "sysguard" || cmd == "systemguard" || cmd == "measuredboot" || cmd == "tbs") { cmdSysGuard(tokens, out); return 0; }
            if (cmd == "vbs" || cmd == "hvci") { cmdVbs(tokens, out); return 0; }
            if (cmd == "dmaguard" || cmd == "dma") { cmdDmaGuard(tokens, out); return 0; }
            if (cmd == "wsl" || cmd == "bash" || cmd == "lxss") { cmdWsl(tokens, out); return 0; }
            if (cmd == "sandbox" || cmd == "wsb") { cmdSandbox(tokens, out); return 0; }
            if (cmd == "whp") { cmdWhp(tokens, out); return 0; }
            if (cmd == "winget" || cmd == "appinstaller") { cmdWinget(tokens, out); return 0; }
            if (cmd == "wdf" || cmd == "kmdf" || cmd == "umdf") { cmdWdf(tokens, out); return 0; }
            if (cmd == "conpty" || cmd == "pty" || cmd == "pseudoconsole") { cmdConpty(tokens, out); return 0; }
            if (cmd == "usb" || cmd == "xhci" || cmd == "winusb") { cmdUsb(tokens, out); return 0; }
            if (cmd == "pci" || cmd == "pcie" || cmd == "lspci") { cmdPci(tokens, out); return 0; }
            if (cmd == "nvme" || cmd == "flash") { cmdNvme(tokens, out); return 0; }
            if (cmd == "acpi" || cmd == "aml") { cmdAcpi(tokens, out); return 0; }
            if (cmd == "hda" || cmd == "hdaudio" || cmd == "azalia") { cmdHda(tokens, out); return 0; }
            if (cmd == "wddm" || cmd == "gpu" || cmd == "graphics") { cmdWddm(tokens, out); return 0; }
            if (cmd == "ndis" || cmd == "nic" || cmd == "razzlenet") { cmdNdis(tokens, out); return 0; }
            if (cmd == "bth" || cmd == "bt" || cmd == "bthport") { cmdBth(tokens, out); return 0; }
            if (cmd == "wifi7" || cmd == "wdiwifi" || cmd == "titanwifi" || cmd == "mlo") { cmdWdiWiFi(tokens, out); return 0; }
            if (cmd == "usb4" || cmd == "thunderbolt" || cmd == "tbt" || cmd == "titanusb4") { cmdUsb4(tokens, out); return 0; }
            if (cmd == "npu" || cmd == "mcdm" || cmd == "titannpu" || cmd == "ai") { cmdNpu(tokens, out); return 0; }
            if (cmd == "cxl" || cmd == "cxlmem" || cmd == "cxlhost" || cmd == "cxlbus" || cmd == "titancxl") { cmdCxl(tokens, out); return 0; }
            if (cmd == "ucsi" || cmd == "usbpd" || cmd == "titanucsi" || cmd == "usbc") { cmdUcsi(tokens, out); return 0; }
            if (cmd == "bypassio" || cmd == "bpio" || cmd == "storqos" || cmd == "titanstorage") { cmdBypassIo(tokens, out); return 0; }
            if (cmd == "pmem" || cmd == "optane" || cmd == "nvdimm" || cmd == "dax" || cmd == "titanpmem") { cmdPmem(tokens, out); return 0; }
            if (cmd == "rdma" || cmd == "roce" || cmd == "infiniband" || cmd == "smbdirect" || cmd == "titanrdma") { cmdRdma(tokens, out); return 0; }
            if (cmd == "pluton" || cmd == "titanpluton" || cmd == "aegispluton") { cmdPluton(tokens, out); return 0; }
            if (cmd == "hfi" || cmd == "director" || cmd == "cppc" || cmd == "titandirector") { cmdHfi(tokens, out); return 0; }
            if (cmd == "cet" || cmd == "shadowstack" || cmd == "titancet" || cmd == "aegiscet") { cmdCet(tokens, out); return 0; }
            if (cmd == "qat" || cmd == "quickassist" || cmd == "titanqat" || cmd == "nexusqat") { cmdQat(tokens, out); return 0; }
            if (cmd == "tee" || cmd == "enclave" || cmd == "sgx" || cmd == "tdx" || cmd == "sevsnp" || cmd == "titantee" || cmd == "aegistee") { cmdTee(tokens, out); return 0; }
            if (cmd == "dsa" || cmd == "iaa" || cmd == "titandsa" || cmd == "nexusdsa") { cmdDsa(tokens, out); return 0; }
            if (cmd == "amx" || cmd == "sme" || cmd == "matrix" || cmd == "titanmatrix" || cmd == "nexusamx") { cmdAmx(tokens, out); return 0; }
            if (cmd == "sriov" || cmd == "sva" || cmd == "pasid" || cmd == "titansriov" || cmd == "nexussva") { cmdSriov(tokens, out); return 0; }
            if (cmd == "iommu" || cmd == "vtd" || cmd == "dmar" || cmd == "titaniommu" || cmd == "aegisiommu") { cmdIommu(tokens, out); return 0; }
            if (cmd == "fwupdate" || cmd == "capsule" || cmd == "uefi" || cmd == "esrt") { cmdFwUpdate(tokens, out); return 0; }
            if (cmd == "standby" || cmd == "modernstandby" || cmd == "pep" || cmd == "sleepstudy") { cmdModernStandby(tokens, out); return 0; }
            if (cmd == "powercfg") { cmdPowerCfg(tokens, out); return 0; }
            if (cmd == "wsa" || cmd == "android" || cmd == "aosp" || cmd == "titanwsa" || cmd == "aegiswsa") { cmdWsa(tokens, out); return 0; }
            if (cmd == "touch" || cmd == "touchpad" || cmd == "ptp" || cmd == "directmanipulation" || cmd == "haptics") { cmdTouchpad(tokens, out); return 0; }
            if (cmd == "ink" || cmd == "stylus" || cmd == "pen" || cmd == "wisptis" || cmd == "handwriting") { cmdInk(tokens, out); return 0; }
            if (cmd == "spatial" || cmd == "spatialaudio" || cmd == "atmos" || cmd == "sonic" || cmd == "apo") { cmdSpatialAudio(tokens, out); return 0; }
            if (cmd == "hpd" || cmd == "presence" || cmd == "sensing" || cmd == "radar" || cmd == "tof") { cmdHpd(tokens, out); return 0; }
            if (cmd == "camera" || cmd == "cam" || cmd == "webcam" || cmd == "cameracx" || cmd == "uvc") { cmdCamera(tokens, out); return 0; }
            if (cmd == "vrr" || cmd == "adaptivesync" || cmd == "gsync" || cmd == "freesync" || cmd == "autohdr") { cmdVrr(tokens, out); return 0; }
            if (cmd == "sensorscx" || cmd == "imu" || cmd == "ahrs" || cmd == "sensorfusion" || cmd == "orientation") { cmdSensorsCx(tokens, out); return 0; }
            if (cmd == "wwan" || cmd == "mbbcx" || cmd == "cellular" || cmd == "5g" || cmd == "lte") { cmdWwan(tokens, out); return 0; }
            if (cmd == "pmp" || cmd == "pavp" || cmd == "hdcp" || cmd == "mfpmp" || cmd == "opm") { cmdPmp(tokens, out); return 0; }
            if (cmd == "vmbus" || cmd == "storvsc" || cmd == "netvsc" || cmd == "hvsock" || cmd == "dmvsc") { cmdVmbus(tokens, out); return 0; }
            if (cmd == "vpci" || cmd == "sriov" || cmd == "dda" || cmd == "pcie") { cmdVpci(tokens, out); return 0; }
            if (cmd == "vsm" || cmd == "vbs" || cmd == "hvci" || cmd == "vtl" || cmd == "credguard") { cmdVsm(tokens, out); return 0; }
            if (cmd == "hotpatch" || cmd == "klp" || cmd == "liveupdate") { cmdHotpatch(tokens, out); return 0; }
            if (cmd == "hyperv" || cmd == "hv" || cmd == "hvr" || cmd == "nestedvm") { cmdHyperv(tokens, out); return 0; }
            if (cmd == "refs" || cmd == "refsutil") { cmdRefs(tokens, out); return 0; }
            if (cmd == "csvfs" || cmd == "csv") { cmdCsvfs(tokens, out); return 0; }
            if (cmd == "wcn" || cmd == "wcifs" || cmd == "hcs" || cmd == "container" || cmd == "docker") { cmdWcn(tokens, out); return 0; }
            if (cmd == "s2d" || cmd == "spaces" || cmd == "storagespaces") { cmdDstorage(tokens, out); return 0; }
            if (cmd == "bcache" || cmd == "branchcache" || cmd == "peerdist" || cmd == "directaccess" || cmd == "da" || cmd == "smbquic" || cmd == "quicfs") { cmdBranchCache(tokens, out); return 0; }
            if (cmd == "sr" || cmd == "storrepl" || cmd == "storagereplica" || cmd == "replica") { cmdStorageReplica(tokens, out); return 0; }
            if (cmd == "cluster" || cmd == "clus" || cmd == "clussvc" || cmd == "failover") { cmdCluster(tokens, out); return 0; }
            if (cmd == "vm" || cmd == "vmms" || cmd == "vswitch" || cmd == "vhdx") { cmdVmms(tokens, out); return 0; }
            if (cmd == "ad" || cmd == "kdc" || cmd == "domain" || cmd == "ds") { cmdActiveDirectory(tokens, out); return 0; }
            if (cmd == "gp" || cmd == "gpo" || cmd == "gpupdate" || cmd == "gpresult") { cmdGroupPolicy(tokens, out); return 0; }
            if (cmd == "rdp" || cmd == "rds" || cmd == "termsrv" || cmd == "wts") { cmdRemoteDesktop(tokens, out); return 0; }
            if (cmd == "nps" || cmd == "ias" || cmd == "radius") { cmdNetworkPolicyServer(tokens, out); return 0; }
            if (cmd == "wsrm" || cmd == "quota" || cmd == "fairshare" || cmd == "dfss") { cmdSystemResourceManager(tokens, out); return 0; }
            if (cmd == "wds" || cmd == "pxe" || cmd == "tftp") { cmdDeploymentServices(tokens, out); return 0; }
            if (cmd == "certsrv" || cmd == "pki" || cmd == "certca") { cmdCertificateServices(tokens, out); return 0; }
            if (cmd == "dns" || cmd == "dnscmd" || cmd == "nslookup") { cmdDnsServer(tokens, out); return 0; }
            if (cmd == "dhcp" || cmd == "dhcpmgmt" || cmd == "netsh_dhcp") { cmdDhcpServer(tokens, out); return 0; }
            if (cmd == "iis" || cmd == "iisreset" || cmd == "appcmd") { cmdIisServer(tokens, out); return 0; }
            if (cmd == "wsus" || cmd == "wsusutil" || cmd == "wuauclt") { cmdWsus(tokens, out); return 0; }
            if (cmd == "winrm" || cmd == "winrs" || cmd == "wsman") { cmdWinRm(tokens, out); return 0; }
            if (cmd == "ssh" || cmd == "sshd" || cmd == "ssh-keygen" || cmd == "ssh-agent" || cmd == "sftp" || cmd == "scp") { cmdSsh(tokens, out); return 0; }
            if (cmd == "lock") { cmdLock(out); return 0; }
            if (cmd == "logoff") { cmdLogoff(out); return 0; }
            if (cmd == "exec" || cmd == "run") {
                if (tokens.size() < 2) {
                    out << "Usage: exec <pe_file_path>\n";
                    return 1;
                }
                return executeBinary(tokens[1], out);
            }
        }

        // Delegate to CmdProcessor for full Windows CMD command & compound operator execution
        int rc = micant::cmd::CmdProcessor::get().executeCompound(line, std::cin, out, out);
        if (rc == -1) return -1;
        if (rc != 9009) {
            return rc;
        }

        // Check if user typed an executable path directly (e.g. unmodified_sample.exe)
        std::string possibleExe = tokens[0];
        if (possibleExe.ends_with(".exe") || canLoadBinary(possibleExe)) {
            return executeBinary(possibleExe, out);
        } else {
            out << "'" << tokens[0] << "' is not recognized as an internal or external command,\n"
                << "operable program or batch file. Type 'help' for commands.\n";
            return 1;
        }
    }

    void runRepl(std::istream& in = std::cin, std::ostream& out = std::cout) {
        printBanner(out);
        std::string line;
        while (true) {
            out << promptString() << std::flush;
            if (!std::getline(in, line)) break;
            int rc = execute(line, out);
            if (rc == -1) {
                out << "Exiting MicaNT Command Shell.\n";
                break;
            }
        }
    }

    bool canLoadBinary(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            std::string withExe = path + ".exe";
            std::ifstream f2(withExe, std::ios::binary);
            return f2.is_open();
        }
        return true;
    }

    int executeBinary(const std::string& binaryPath, std::ostream& out = std::cout) {
        std::string target = binaryPath;
        std::ifstream file(target, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            if (!target.ends_with(".exe")) {
                target += ".exe";
                file.open(target, std::ios::binary | std::ios::ate);
            }
        }
        if (!file.is_open()) {
            // Try relative to bin/ or System32
            std::string inBin = "bin/" + target;
            file.open(inBin, std::ios::binary | std::ios::ate);
            if (file.is_open()) target = inBin;
        }

        if (!file.is_open()) {
            out << "Error: The system cannot find the file specified: " << binaryPath << "\n";
            return 2;
        }

        std::streamsize fileSize = file.tellg();
        if (fileSize <= 0) {
            out << "Error: Executable file is empty.\n";
            return 3;
        }
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> fileBytes(static_cast<size_t>(fileSize));
        file.read(reinterpret_cast<char*>(fileBytes.data()), fileSize);
        file.close();

        // 1. Inspect PE Headers
        pe::ImageNtHeaders64 headers{};
        std::vector<pe::ImageSectionHeader> sections;
        NtStatus stInspect = pe::PeLoader::inspect(fileBytes, headers, sections);
        if (!NT_SUCCESS(stInspect)) {
            out << "Error: File is not a valid 64-bit Windows PE executable.\n";
            return 4;
        }

        // 2. Parse Import Directory
        std::vector<pe::ImportedLibrary> imports;
        pe::PeLoader::parseImports(fileBytes, headers, sections, imports);

        // 3. Map Image
        size_t imageSize = headers.optionalHeader.sizeOfImage;
        uint8_t* mappedBase = reinterpret_cast<uint8_t*>(
            host_mem::VirtualAlloc(reinterpret_cast<void*>(headers.optionalHeader.imageBase), imageSize, 0x1000 | 0x2000, 0x40)
        );
        if (!mappedBase) {
            mappedBase = reinterpret_cast<uint8_t*>(
                host_mem::VirtualAlloc(nullptr, imageSize, 0x1000 | 0x2000, 0x40)
            );
        }
        if (!mappedBase) {
            out << "Error: Out of memory mapping executable image.\n";
            return 5;
        }

        win32::g_CurrentExecutableBase = reinterpret_cast<uintptr_t>(mappedBase);
        auto* peb = ntdll::RtlGetCurrentPeb();
        if (peb) {
            peb->imageBaseAddress = reinterpret_cast<uint64_t>(mappedBase);
        }

        NtStatus stMap = pe::PeLoader::mapImage(fileBytes, headers, sections, mappedBase, imageSize);
        if (!NT_SUCCESS(stMap)) {
            host_mem::VirtualFree(mappedBase, 0, 0x8000);
            out << "Error: Failed to map PE sections to virtual addresses.\n";
            return 6;
        }

        // Intercept ExitProcess, TerminateProcess, and CRT exit so child binary terminates only its worker thread
        void* origExit = ldr::DynamicLoader::get().getExport("kernel32.dll", "ExitProcess");
        void* origTerminate = ldr::DynamicLoader::get().getExport("kernel32.dll", "TerminateProcess");
        void* origMsvcrtExit = ldr::DynamicLoader::get().getExport("msvcrt.dll", "exit");
        void* origMsvcrt_Exit = ldr::DynamicLoader::get().getExport("msvcrt.dll", "_exit");
        s_LastBinaryExitCode.store(0);
        s_LastBinaryExited.store(false);
        ldr::DynamicLoader::get().registerExport("kernel32.dll", "ExitProcess", reinterpret_cast<void*>(ShellBinaryExitProcess));
        ldr::DynamicLoader::get().registerExport("kernel32.dll", "TerminateProcess", reinterpret_cast<void*>(ShellBinaryTerminateProcess));
        ldr::DynamicLoader::get().registerExport("msvcrt.dll", "exit", reinterpret_cast<void*>(ShellBinaryExitProcess));
        ldr::DynamicLoader::get().registerExport("msvcrt.dll", "_exit", reinterpret_cast<void*>(ShellBinaryExitProcess));
        ldr::DynamicLoader::get().registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "exit", reinterpret_cast<void*>(ShellBinaryExitProcess));
        ldr::DynamicLoader::get().registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_exit", reinterpret_cast<void*>(ShellBinaryExitProcess));

        // 4. Bind Imports
        pe::PeLoader::bindImports(
            mappedBase,
            imports,
            [&out](std::string_view mod, std::string_view fn, uint16_t /*ord*/) -> void* {
                void* p = ldr::DynamicLoader::get().getExport(mod, fn);
                if (!p) {
                    out << "[Shell Warning] Unresolved import: " << mod << "!" << fn << "\n";
                }
                return p;
            }
        );

        // 5. Apply Relocations
        pe::PeLoader::applyRelocations(mappedBase, imageSize, headers, reinterpret_cast<uintptr_t>(mappedBase));

        // 6. Launch entry point in isolated execution thread
        using EntryFunc = void(*)();
        auto fnEntry = reinterpret_cast<EntryFunc>(mappedBase + headers.optionalHeader.addressOfEntryPoint);

        out << "[Shell] Launching '" << target << "' (ImageBase: 0x" 
            << std::hex << reinterpret_cast<uintptr_t>(mappedBase) 
            << ", Entry: 0x" << reinterpret_cast<uintptr_t>(fnEntry) << std::dec << ")...\n" << std::flush;

        auto threadProc = [](void* param) -> unsigned long {
            auto entry = reinterpret_cast<EntryFunc>(param);
#if defined(_MSC_VER)
            __try {
                entry();
            } __except (1) {
            }
#else
            try {
                entry();
            } catch (...) {
            }
#endif
            return 0;
        };

        void* hThread = host_thread::CreateThread(
            nullptr, 0,
            reinterpret_cast<uint32_t(__stdcall*)(void*)>(+threadProc),
            reinterpret_cast<void*>(fnEntry),
            0, nullptr
        );

        uint32_t exitCode = 0;
        if (hThread) {
            host_thread::WaitForSingleObject(hThread, 0xFFFFFFFF);
            host_thread::GetExitCodeThread(hThread, &exitCode);
            host_thread::CloseHandle(hThread);
        }

        // Restore original exports
        if (origExit) ldr::DynamicLoader::get().registerExport("kernel32.dll", "ExitProcess", origExit);
        if (origTerminate) ldr::DynamicLoader::get().registerExport("kernel32.dll", "TerminateProcess", origTerminate);
        if (origMsvcrtExit) {
            ldr::DynamicLoader::get().registerExport("msvcrt.dll", "exit", origMsvcrtExit);
            ldr::DynamicLoader::get().registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "exit", origMsvcrtExit);
        }
        if (origMsvcrt_Exit) {
            ldr::DynamicLoader::get().registerExport("msvcrt.dll", "_exit", origMsvcrt_Exit);
            ldr::DynamicLoader::get().registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_exit", origMsvcrt_Exit);
        }

        if (s_LastBinaryExited.load()) {
            exitCode = s_LastBinaryExitCode.load();
        }

        out << "[Shell] Process finished with exit code " << exitCode << ".\n";
        host_mem::VirtualFree(mappedBase, 0, 0x8000);
        win32::g_CurrentExecutableBase = 0;
        return static_cast<int>(exitCode);
    }

private:

    // ------------------------------------------------------------------------
    // Modular Domain Command Handlers (include/micant/shell/*.hpp)
    // ------------------------------------------------------------------------
#include "shell/core_commands.hpp"
#include "shell/net_commands.hpp"
#include "shell/media_commands.hpp"
#include "shell/security_commands.hpp"
#include "shell/system_commands.hpp"
#include "shell/hardware_commands.hpp"
#include "shell/storage_commands.hpp"
#include "shell/server_commands.hpp"

    static std::string trim(std::string_view s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return std::string(s.substr(start, end - start + 1));
    }

    static std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char ch = line[i];
            if (ch == '"') {
                inQuotes = !inQuotes;
                current += ch;
            } else if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else {
                current += ch;
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }
};

} // namespace micant::shell

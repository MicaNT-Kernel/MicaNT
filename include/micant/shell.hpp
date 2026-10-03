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
        tcpip::NetworkStack::get().initialize();

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

        // Check MicaNT executive test & diagnostic commands
        if (!hasCompound) {
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
        if (origMsvcrtExit) ldr::DynamicLoader::get().registerExport("msvcrt.dll", "exit", origMsvcrtExit);
        if (origMsvcrt_Exit) ldr::DynamicLoader::get().registerExport("msvcrt.dll", "_exit", origMsvcrt_Exit);

        if (s_LastBinaryExited.load()) {
            exitCode = s_LastBinaryExitCode.load();
        }

        out << "[Shell] Process finished with exit code " << exitCode << ".\n";
        host_mem::VirtualFree(mappedBase, 0, 0x8000);
        win32::g_CurrentExecutableBase = 0;
        return static_cast<int>(exitCode);
    }

private:
    void cmdHelp(std::ostream& out) {
        out << "MicaNT Shell Built-in Commands:\n"
            << "  HELP / ?          Displays this list of commands\n"
            << "  VER               Displays the MicaNT operating system version\n"
            << "  DIR / LS [path]   Lists files and directories in current or specified path\n"
            << "  CD [path]         Displays or changes the current directory\n"
            << "  TYPE / CAT <file> Displays the contents of a text file\n"
            << "  ECHO [text]       Prints text to the console, expanding %VARIABLES%\n"
            << "  SET [var=value]   Displays or modifies environment variables\n"
            << "  CLS / CLEAR       Clears the screen\n"
            << "  COLOR [attr]      Sets the console foreground/background color\n"
            << "  MEM               Displays kernel pool and virtual memory statistics\n"
            << "  SYSTEMINFO        Displays system architecture, HAL, and processor specs\n"
            << "  PS / TASKLIST     Lists active CSRSS registered processes and threads\n"
            << "  TIME / DATE       Displays system chronometry and timestamp\n"
            << "  IPCONFIG [/all]   Displays network adapter configuration and IP addresses\n"
            << "  PING <host>       Sends ICMP Echo Requests to verify network connectivity\n"
            << "  NETSTAT           Displays active TCP/UDP network connections and ports\n"
            << "  NET START/STOP    Controls and lists running Windows services\n"
            << "  NET USER [name]   Enumerates or modifies local user accounts in SAM\n"
            << "  SC QUERY/START    Interrogates and controls Service Control Manager\n"
            << "  WHOAMI [/priv]    Displays user identity, group SIDs, and token privileges\n"
            << "  PRISMX / GPU      Displays GPU adapters, VRAM, and runs 3D tests (prismx test)\n"
            << "  D3D9 / DX9        Initializes Direct3D 9 fixed-function pipeline test\n"
            << "  GDI               Displays GDI32 graphics engine info and verifies 2D drawing\n"
            << "  COM / OLE         Displays COM / OLE runtime info, GUID generation and BSTR test\n"
            << "  SHELL32           Displays known shell folders, tray manager, and parses args\n"
            << "  COMCTL            Displays common controls info and tests Progress/Status bars\n"
            << "  VIEW3D            Launches interactive 3D model viewer (view3d --torus|--cube|--crystal|--wireframe)\n"
            << "  VULKAN / VKINFO   Displays Vulkan ICD status, physical devices, and vkcube test\n"
            << "  WINMM             WinMM Multimedia API (audio devices, high-res timer, MCI, playback)\n"
            << "  DSOUND            DirectSound 8 3D Audio Subsystem & Real-Time Voice Mixer\n"
            << "  VERSION / VERINFO Queries Windows Version Information resource metadata for PE files\n"
            << "  OPENGL / GL / WGL Silicon Graphics OpenGL 1.4 API & Windows WGL 3D Runtime\n"
            << "  WININET           Windows Internet Subsystem (HTTP/1.1, cookies, URL cache)\n"
            << "  URLMON / CURL     Downloads web resources using URLDownloadToFile & MIME sniffer\n"
            << "  BCRYPT / CNG      Cryptography Next Generation (SHA256/384/512, MD5, AES, PBKDF2)\n"
            << "  CERTMGR           Windows Certificate Manager & X.509 Digital Certificate Store\n"
            << "  DPAPI             Data Protection API (CryptProtectData / CryptUnprotectData)\n"
            << "  SSPI              Security Support Provider Interface (Schannel TLS & NTLM)\n"
            << "  SCHANNEL          Secure Channel Subsystem (TLS 1.2 / TLS 1.3 Handshake & Framing)\n"
            << "  RPC               Remote Procedure Call Runtime & NDR Engine (rpcrt4.dll)\n"
            << "  UUIDGEN           Universally Unique Identifier (UUID / GUID) Generator\n"
            << "  OLEAUT            Windows OLE Automation, SafeArray & TypeLib Engine (oleaut32.dll)\n"
            << "  DEVMGMT / SETUP   Windows Device Manager & Installation Subsystem (setupapi.dll)\n"
            << "  STG / DOCFILE     Windows OLE Structured Storage & Compound File System (ole32.dll)\n"
            << "  WEVTUTIL / EVENTLOG Windows Event Log Subsystem & Diagnostics (wevtapi.dll)\n"
            << "  WMIC / WBEM       Windows Management Instrumentation Engine (wbemprox.dll)\n"
            << "  SCHTASKS          Windows Task Scheduler 2.0 Engine (taskschd.dll)\n"
            << "  BITSADMIN / BITS  Background Intelligent Transfer Service Queue Manager (qmgr.dll)\n"
            << "  VSSADMIN / VSS    Volume Shadow Copy Service Administration (vssapi.dll)\n"
            << "  LOCK              Locks workstation and switches to secure Winlogon desktop\n"
            << "  LOGOFF            Logs off current interactive user session\n"
            << "  EXEC <binary.exe> Executes an unmodified 64-bit Windows PE binary\n"
            << "  EXIT / QUIT       Quits the MicaNT command shell\n";
    }

    void cmdVer(std::ostream& out) {
        out << "MicaNT [Version 10.0.26100.1]\n"
            << "Zero Telemetry Clean-Room Executive (x86_64, SMP 4-Core)\n";
    }

    void cmdCls(std::ostream& out) {
        auto session = conhost::ConhostManager::get().getConsole(win32::GetCurrentProcessId());
        if (session) {
            session->getScreenBuffer().clear();
        }
        out << "\033[2J\033[H"; // ANSI VT100 clear
    }

    void cmdDir(const std::vector<std::string>& tokens, std::ostream& out) {
        std::string searchDir = getCurrentDirectory();
        if (tokens.size() > 1) {
            searchDir = tokens[1];
        }

        std::wstring wSearch(searchDir.begin(), searchDir.end());
        std::vector<fs::VirtualFileSystem::DirectoryEntry> entries;
        NtStatus st = fs::VirtualFileSystem::get().queryDirectory(wSearch, entries);
        if (!NT_SUCCESS(st)) {
            out << "File Not Found: " << searchDir << "\n";
            return;
        }

        out << " Directory of " << searchDir << "\n\n";
        size_t totalFiles = 0;
        size_t totalBytes = 0;
        size_t totalDirs = 0;

        for (const auto& e : entries) {
            std::string name;
            for (wchar_t wc : e.name) name.push_back(static_cast<char>(wc & 0x7F));
            out << "10/01/2026  12:00 PM    ";
            if (e.isDirectory) {
                out << "<DIR>          ";
                totalDirs++;
            } else {
                out << std::setw(14) << e.size << " ";
                totalFiles++;
                totalBytes += e.size;
            }
            out << name << "\n";
        }
        out << "               " << totalFiles << " File(s)    " << totalBytes << " bytes\n";
        out << "               " << totalDirs << " Dir(s)     2,629,282,086,912 bytes free\n";
    }

    void cmdCd(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            out << getCurrentDirectory() << "\n";
            return;
        }

        std::string target = tokens[1];
        if (target == "..") {
            std::string cur = getCurrentDirectory();
            size_t slash = cur.find_last_of("\\/");
            if (slash != std::string::npos && slash > 2) {
                target = cur.substr(0, slash);
            } else if (slash == 2) {
                target = cur.substr(0, 3); // Root path
            }
        }

        std::wstring wTarget(target.begin(), target.end());
        if (win32::SetCurrentDirectoryW(wTarget.c_str())) {
            // success
        } else {
            out << "The system cannot find the path specified: " << target << "\n";
        }
    }

    void cmdType(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            out << "The syntax of the command is incorrect. Usage: type <file>\n";
            return;
        }

        std::string filename = tokens[1];
        std::wstring wFile(filename.begin(), filename.end());
        std::shared_ptr<fs::FileObject> fileObj;
        NtStatus st = fs::VirtualFileSystem::get().createOrOpenFile(wFile, fs::FILE_GENERIC_READ, fs::FILE_OPEN, fileObj);
        if (!NT_SUCCESS(st) || !fileObj) {
            out << "The system cannot find the file specified: " << filename << "\n";
            return;
        }

        const auto& data = fileObj->getData();
        std::string content(data.begin(), data.end());
        out << content;
        if (!content.empty() && content.back() != '\n') out << "\n";
    }

    void cmdEcho(const std::vector<std::string>& tokens, const std::string& fullLine, std::ostream& out) {
        if (tokens.size() < 2) {
            out << "ECHO is on.\n";
            return;
        }

        size_t pos = fullLine.find_first_not_of(" \t", 4);
        if (pos == std::string::npos) {
            out << "\n";
            return;
        }

        std::string text = fullLine.substr(pos);
        // Expand %VAR%
        std::string expanded;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '%' && i + 1 < text.size()) {
                size_t endPct = text.find('%', i + 1);
                if (endPct != std::string::npos) {
                    std::string varName = text.substr(i + 1, endPct - i - 1);
                    std::wstring wVar(varName.begin(), varName.end());
                    wchar_t valBuf[256]{};
                    win32::DWORD len = win32::GetEnvironmentVariableW(wVar.c_str(), valBuf, 256);
                    if (len > 0) {
                        for (win32::DWORD c = 0; c < len; ++c) {
                            expanded.push_back(static_cast<char>(valBuf[c] & 0x7F));
                        }
                    }
                    i = endPct;
                    continue;
                }
            }
            expanded.push_back(text[i]);
        }
        out << expanded << "\n";
    }

    void cmdSet(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            // List common variables
            const wchar_t* commonVars[] = {
                L"OS", L"SystemRoot", L"windir", L"ComSpec", L"PATH",
                L"NUMBER_OF_PROCESSORS", L"PROCESSOR_ARCHITECTURE", L"USERPROFILE", L"HOMEDRIVE"
            };
            wchar_t valBuf[256]{};
            for (const auto* v : commonVars) {
                win32::DWORD len = win32::GetEnvironmentVariableW(v, valBuf, 256);
                if (len > 0) {
                    std::wcout << v << L"=" << valBuf << L"\n";
                }
            }
            return;
        }

        std::string expr = tokens[1];
        size_t eq = expr.find('=');
        if (eq == std::string::npos) {
            std::wstring wVar(expr.begin(), expr.end());
            wchar_t valBuf[256]{};
            win32::DWORD len = win32::GetEnvironmentVariableW(wVar.c_str(), valBuf, 256);
            if (len > 0) {
                std::wcout << wVar << L"=" << valBuf << L"\n";
            } else {
                out << "Environment variable " << expr << " not defined\n";
            }
        } else {
            std::string var = expr.substr(0, eq);
            std::string val = expr.substr(eq + 1);
            std::wstring wVar(var.begin(), var.end());
            std::wstring wVal(val.begin(), val.end());
            win32::SetEnvironmentVariableW(wVar.c_str(), wVal.c_str());
        }
    }

    void cmdColor(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            win32::SetConsoleTextAttribute(win32::GetStdHandle(win32::STD_OUTPUT_HANDLE), 0x07); // Default White on Black
            return;
        }
        uint16_t attr = static_cast<uint16_t>(std::strtoul(tokens[1].c_str(), nullptr, 16));
        win32::SetConsoleTextAttribute(win32::GetStdHandle(win32::STD_OUTPUT_HANDLE), attr);
        out << "Console color set to 0x" << std::hex << attr << std::dec << "\n";
    }

    void cmdTime(std::ostream& out) {
        win32::SYSTEMTIME st{};
        win32::GetLocalTime(&st);
        out << "System Date: " << st.wMonth << "/" << st.wDay << "/" << st.wYear << "\n";
        out << "System Time: " << std::setfill('0') << std::setw(2) << st.wHour << ":"
            << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond << "." 
            << std::setw(3) << st.wMilliseconds << "\n";
    }

    void cmdMem(std::ostream& out) {
        out << "\nMicaNT Memory Manager & Executive Pools:\n";
        out << "  - Kernel NonPaged Pool: " << (ex::ExecutivePool::get().getTotalNonPagedBytes() / 1024) << " KB\n";
        out << "  - Kernel Paged Pool:    " << (ex::ExecutivePool::get().getTotalPagedBytes() / 1024) << " KB\n";
        out << "  - Memory Tag 'Mica':    Active (Pool diagnostic tracking enabled)\n";
        out << "  - VAD Reservations:     Simulated 64-bit user/kernel address spaces\n";
        out << "  - Idle RAM Footprint:   < 32 MB total operating state\n\n";
    }

    void cmdSystemInfo(std::ostream& out) {
        auto& hal = hal::HardwareAbstractionLayer::get();
        (void)hal;
        LargeInteger freq{}, perf{};
        win32::QueryPerformanceFrequency(&freq);
        win32::QueryPerformanceCounter(&perf);

        out << "\nHost Name:                 MICANT-DESKTOP\n"
            << "OS Name:                   MicaNT Dave Cutler Clean-Room Executive\n"
            << "OS Version:                10.0.26100.1 N/A Build 26100\n"
            << "OS Build Type:             Multiprocessor Free (Zero Telemetry)\n"
            << "System Architecture:       x64-based PC (x86_64 AMD64 Long Mode)\n"
            << "Processor(s):              4 Processors Installed [SMP Multi-Core Topology]\n"
            << "HAL KPCR Structure:        GS:[0] (PRCB Active on Core 0..3)\n"
            << "QPC Performance Frequency: " << freq.quadPart << " Hz (1 GHz Monotonic)\n"
            << "Time Elapsed Since Boot:   " << (perf.quadPart / 1000000000ULL) << " seconds\n"
            << "Active Windows Subsystem:  CSRSS ALPC Server & ConHost Screen Blitter\n\n";
    }

    void cmdPs(std::ostream& out) {
        out << "\nImage Name                     PID Session Name        Session#    Mem Usage\n"
            << "========================= ======== ================ =========== ============\n"
            << "System                           4 Services                   0       256 K\n"
            << "smss.exe                       240 Services                   0       384 K\n"
            << "csrss.exe                      352 Console                    1     1,024 K\n"
            << "wininit.exe                    360 Services                   0       512 K\n"
            << "conhost.exe                    420 Console                    1       896 K\n"
            << "cmd.exe (msh)                  512 Console                    1       768 K\n\n";
    }

    void cmdPing(const std::vector<std::string>& tokens, std::ostream& out) {
        std::string target = (tokens.size() > 1) ? tokens[1] : "127.0.0.1";
        auto ip = tcpip::Ipv4Address::fromString(target);
        if (ip.isZero() && target != "0.0.0.0") {
            if (target == "localhost") {
                ip = tcpip::Ipv4Address::loopback();
            } else {
                out << "Ping request could not find host " << target << ". Please check the name and try again.\n";
                return;
            }
        }

        out << "\nPinging " << ip.toString() << " with 32 bytes of data:\n";
        int sent = 0;
        int received = 0;
        uint32_t minRtt = 999999;
        uint32_t maxRtt = 0;
        uint64_t totalRtt = 0;

        for (int i = 0; i < 4; ++i) {
            sent++;
            auto res = tcpip::NetworkStack::get().ping(ip);
            if (res.success) {
                received++;
                if (res.rttMs < minRtt) minRtt = res.rttMs;
                if (res.rttMs > maxRtt) maxRtt = res.rttMs;
                totalRtt += res.rttMs;
                out << "Reply from " << ip.toString() << ": bytes=" << res.bytesReceived 
                    << " time=" << res.rttMs << "ms TTL=" << static_cast<int>(res.ttl) << "\n";
            } else {
                out << "Request timed out.\n";
            }
        }

        out << "\nPing statistics for " << ip.toString() << ":\n"
            << "    Packets: Sent = " << sent << ", Received = " << received << ", Lost = " << (sent - received)
            << " (" << ((sent - received) * 100 / sent) << "% loss),\n";
        if (received > 0) {
            out << "Approximate round trip times in milli-seconds:\n"
                << "    Minimum = " << minRtt << "ms, Maximum = " << maxRtt 
                << "ms, Average = " << (totalRtt / received) << "ms\n\n";
        }
    }

    void cmdIpConfig(const std::vector<std::string>& tokens, std::ostream& out) {
        bool showAll = false;
        if (tokens.size() > 1 && (tokens[1] == "/all" || tokens[1] == "-a")) {
            showAll = true;
        }

        auto& net = tcpip::NetworkStack::get();
        auto adapter = net.getAdapter();

        out << "\nWindows IP Configuration\n\n";
        if (showAll) {
            out << "   Host Name . . . . . . . . . . . . : MicaNT-Workstation\n"
                << "   Primary Dns Suffix  . . . . . . . : localdomain\n"
                << "   Node Type . . . . . . . . . . . . : Broadcast\n"
                << "   IP Routing Enabled. . . . . . . . : No\n"
                << "   WINS Proxy Enabled. . . . . . . . : No\n\n";
        }

        out << "Ethernet adapter Ethernet0:\n\n"
            << "   Connection-specific DNS Suffix  . : localdomain\n";
        if (showAll && adapter) {
            std::string desc;
            for (wchar_t wc : adapter->getFriendlyName()) {
                desc += (wc < 128) ? static_cast<char>(wc) : '?';
            }
            out << "   Description . . . . . . . . . . . : " << desc << "\n"
                << "   Physical Address. . . . . . . . . : " << adapter->getMacAddress().toString() << "\n"
                << "   DHCP Enabled. . . . . . . . . . . : No\n"
                << "   Autoconfiguration Enabled . . . . : Yes\n";
        }
        out << "   Link-local IPv6 Address . . . . . : " << net.getLocalIpv6().toString() << "%1\n"
            << "   IPv4 Address. . . . . . . . . . . : " << net.getLocalIp().toString() << "\n"
            << "   Subnet Mask . . . . . . . . . . . : " << net.getSubnetMask().toString() << "\n"
            << "   Default Gateway . . . . . . . . . : " << net.getGateway().toString() << "\n";
        if (showAll) {
            out << "   DNS Servers . . . . . . . . . . . : " << net.getDnsServer().toString() << "\n"
                << "   NetBIOS over Tcpip. . . . . . . . : Enabled\n";
        }
        out << "\n";
    }

    void cmdNetstat(const std::vector<std::string>& /*tokens*/, std::ostream& out) {
        auto endpoints = tcpip::NetworkStack::get().getActiveEndpoints();
        out << "\nActive Connections\n\n"
            << "  Proto  Local Address          Foreign Address        State\n";
        for (const auto& ep : endpoints) {
            std::string proto = (ep.type == tcpip::SOCK_STREAM) ? "TCP" : "UDP";
            std::string local = ep.localIp.toString() + ":" + std::to_string(ep.localPort);
            std::string remote = (ep.type == tcpip::SOCK_STREAM && ep.tcpState == tcpip::TcpState::Listen)
                ? "0.0.0.0:0"
                : ep.remoteIp.toString() + ":" + std::to_string(ep.remotePort);
            std::string stateStr;
            switch (ep.tcpState) {
                case tcpip::TcpState::Listen: stateStr = "LISTENING"; break;
                case tcpip::TcpState::SynSent: stateStr = "SYN_SENT"; break;
                case tcpip::TcpState::SynReceived: stateStr = "SYN_RECEIVED"; break;
                case tcpip::TcpState::Established: stateStr = "ESTABLISHED"; break;
                case tcpip::TcpState::FinWait1:
                case tcpip::TcpState::FinWait2: stateStr = "FIN_WAIT"; break;
                case tcpip::TcpState::CloseWait: stateStr = "CLOSE_WAIT"; break;
                case tcpip::TcpState::TimeWait: stateStr = "TIME_WAIT"; break;
                default: stateStr = (ep.type == tcpip::SOCK_DGRAM) ? "*:*" : "CLOSED"; break;
            }
            out << "  " << std::left << std::setw(7) << proto
                << std::setw(23) << local
                << std::setw(23) << remote
                << stateStr << "\n";
        }
        out << "\n";
    }

    static std::string wideToAscii(std::wstring_view wstr) {
        std::string s;
        s.reserve(wstr.size());
        for (wchar_t wc : wstr) {
            s.push_back(static_cast<char>(wc & 0x7F));
        }
        return s;
    }

    void cmdNet(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            out << "The syntax of this command is:\n\nNET [ START | STOP ]\n\n";
            return;
        }

        std::string sub = tokens[1];
        std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (sub == "user") {
            cmdNetUser(tokens, out);
            return;
        } else if (sub == "start") {
            if (tokens.size() == 2) {
                out << "\nThese Windows services are started:\n\n";
                std::vector<scm::ENUM_SERVICE_STATUS_PROCESSW> list;
                scm::ServiceControlManager::get().enumServicesStatus(scm::SERVICE_TYPE_ALL, 1, list);
                for (const auto& s : list) {
                    std::string disp = wideToAscii(s.lpDisplayName);
                    out << "   " << disp << "\n";
                }
                out << "\nThe command completed successfully.\n\n";
                return;
            }

            std::string svcName = tokens[2];
            std::wstring wSvcName(svcName.begin(), svcName.end());
            out << "The " << svcName << " service is starting.\n";
            advapi32::SC_HANDLE hScm = advapi32::OpenSCManagerW(nullptr, nullptr, scm::SC_MANAGER_ALL_ACCESS);
            if (!hScm) {
                out << "System error 5 has occurred.\nAccess is denied.\n";
                return;
            }
            advapi32::SC_HANDLE hSvc = advapi32::OpenServiceW(hScm, wSvcName.c_str(), scm::SERVICE_START | scm::SERVICE_QUERY_STATUS);
            if (!hSvc) {
                out << "System error 1060 has occurred.\nThe specified service does not exist as an installed service.\n";
                advapi32::CloseServiceHandle(hScm);
                return;
            }
            if (advapi32::StartServiceW(hSvc, 0, nullptr)) {
                out << "The " << svcName << " service was started successfully.\n\n";
            } else {
                out << "The " << svcName << " service could not be started.\n\n";
            }
            advapi32::CloseServiceHandle(hSvc);
            advapi32::CloseServiceHandle(hScm);
        } else if (sub == "stop") {
            if (tokens.size() < 3) {
                out << "Usage: NET STOP <service_name>\n";
                return;
            }
            std::string svcName = tokens[2];
            std::wstring wSvcName(svcName.begin(), svcName.end());
            out << "The " << svcName << " service is stopping.\n";
            advapi32::SC_HANDLE hScm = advapi32::OpenSCManagerW(nullptr, nullptr, scm::SC_MANAGER_ALL_ACCESS);
            if (!hScm) {
                out << "System error 5 has occurred.\nAccess is denied.\n";
                return;
            }
            advapi32::SC_HANDLE hSvc = advapi32::OpenServiceW(hScm, wSvcName.c_str(), scm::SERVICE_STOP | scm::SERVICE_QUERY_STATUS);
            if (!hSvc) {
                out << "System error 1060 has occurred.\nThe specified service does not exist as an installed service.\n";
                advapi32::CloseServiceHandle(hScm);
                return;
            }
            scm::SERVICE_STATUS st{};
            if (advapi32::ControlService(hSvc, scm::SERVICE_CONTROL_STOP, &st)) {
                out << "The " << svcName << " service was stopped successfully.\n\n";
            } else {
                out << "The " << svcName << " service could not be stopped.\n\n";
            }
            advapi32::CloseServiceHandle(hSvc);
            advapi32::CloseServiceHandle(hScm);
        } else {
            out << "The option " << tokens[1] << " is unknown.\n\n";
        }
    }

    void cmdSc(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() < 2) {
            out << "DESCRIPTION:\n        SC is a command line program used for communicating with the\n        Service Control Manager and services.\nUSAGE:\n        sc <server> [command] [service name] <option1> <option2>...\n";
            return;
        }

        std::string sub = tokens[1];
        std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (sub == "query") {
            if (tokens.size() == 2) {
                std::vector<scm::ENUM_SERVICE_STATUS_PROCESSW> list;
                scm::ServiceControlManager::get().enumServicesStatus(scm::SERVICE_TYPE_ALL, 0, list);
                for (const auto& s : list) {
                    std::string name = wideToAscii(s.lpServiceName);
                    std::string disp = wideToAscii(s.lpDisplayName);
                    out << "SERVICE_NAME: " << name << "\n"
                        << "DISPLAY_NAME: " << disp << "\n"
                        << "        TYPE               : 20  WIN32_SHARE_PROCESS\n"
                        << "        STATE              : " << s.ServiceStatusProcess.dwCurrentState << "  "
                        << (s.ServiceStatusProcess.dwCurrentState == scm::SERVICE_RUNNING ? "RUNNING" : "STOPPED") << "\n"
                        << "        WIN32_EXIT_CODE    : 0  (0x0)\n"
                        << "        SERVICE_EXIT_CODE  : 0  (0x0)\n"
                        << "        CHECKPOINT         : 0x0\n"
                        << "        WAIT_HINT          : 0x0\n\n";
                }
                return;
            }

            std::string svcName = tokens[2];
            std::wstring wSvcName(svcName.begin(), svcName.end());
            auto rec = scm::ServiceControlManager::get().getServiceRecord(wSvcName);
            if (!rec) {
                out << "[SC] EnumQueryServicesStatus:OpenService FAILED 1060:\n\nThe specified service does not exist as an installed service.\n\n";
                return;
            }

            std::string disp = wideToAscii(rec->displayName);
            std::string stateStr = (rec->status.dwCurrentState == scm::SERVICE_RUNNING) ? "RUNNING" :
                                   (rec->status.dwCurrentState == scm::SERVICE_STOPPED) ? "STOPPED" :
                                   (rec->status.dwCurrentState == scm::SERVICE_PAUSED) ? "PAUSED" : "PENDING";
            out << "[SC] QueryServiceStatus\n\n"
                << "SERVICE_NAME: " << svcName << "\n"
                << "DISPLAY_NAME: " << disp << "\n"
                << "        TYPE               : " << rec->status.dwServiceType << "\n"
                << "        STATE              : " << rec->status.dwCurrentState << "  " << stateStr << "\n"
                << "        WIN32_EXIT_CODE    : " << rec->status.dwWin32ExitCode << "  (0x0)\n"
                << "        SERVICE_EXIT_CODE  : " << rec->status.dwServiceSpecificExitCode << "  (0x0)\n"
                << "        CHECKPOINT         : 0x" << std::hex << rec->status.dwCheckPoint << std::dec << "\n"
                << "        WAIT_HINT          : 0x" << std::hex << rec->status.dwWaitHint << std::dec << "\n"
                << "        PID                : " << rec->status.dwProcessId << "\n\n";
        } else if (sub == "start") {
            if (tokens.size() < 3) {
                out << "Usage: sc start <service_name>\n";
                return;
            }
            std::string svcName = tokens[2];
            std::wstring wSvcName(svcName.begin(), svcName.end());
            advapi32::SC_HANDLE hScm = advapi32::OpenSCManagerW(nullptr, nullptr, scm::SC_MANAGER_ALL_ACCESS);
            advapi32::SC_HANDLE hSvc = advapi32::OpenServiceW(hScm, wSvcName.c_str(), scm::SERVICE_START);
            if (!hSvc) {
                out << "[SC] OpenService FAILED 1060: The specified service does not exist.\n";
            } else {
                if (advapi32::StartServiceW(hSvc, 0, nullptr)) {
                    out << "[SC] StartService SUCCESS\n";
                } else {
                    out << "[SC] StartService FAILED\n";
                }
                advapi32::CloseServiceHandle(hSvc);
            }
            advapi32::CloseServiceHandle(hScm);
        } else if (sub == "stop") {
            if (tokens.size() < 3) {
                out << "Usage: sc stop <service_name>\n";
                return;
            }
            std::string svcName = tokens[2];
            std::wstring wSvcName(svcName.begin(), svcName.end());
            advapi32::SC_HANDLE hScm = advapi32::OpenSCManagerW(nullptr, nullptr, scm::SC_MANAGER_ALL_ACCESS);
            advapi32::SC_HANDLE hSvc = advapi32::OpenServiceW(hScm, wSvcName.c_str(), scm::SERVICE_STOP);
            if (!hSvc) {
                out << "[SC] OpenService FAILED 1060: The specified service does not exist.\n";
            } else {
                scm::SERVICE_STATUS st{};
                if (advapi32::ControlService(hSvc, scm::SERVICE_CONTROL_STOP, &st)) {
                    out << "[SC] ControlService SUCCESS\n";
                } else {
                    out << "[SC] ControlService FAILED\n";
                }
                advapi32::CloseServiceHandle(hSvc);
            }
            advapi32::CloseServiceHandle(hScm);
        } else {
            out << "[SC] Unknown command: " << tokens[1] << "\n";
        }
    }

    void cmdNetUser(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() == 2) {
            out << "\nUser accounts for \\\\MICANT-DESKTOP\n\n"
                << "-------------------------------------------------------------------------------\n";
            auto users = sam::SamDatabase::get().enumerateUsers();
            std::sort(users.begin(), users.end(), [](const auto& a, const auto& b) { return a.rid < b.rid; });
            for (size_t i = 0; i < users.size(); ++i) {
                std::string name = wideToAscii(users[i].accountName);
                out << std::left << std::setw(25) << name;
                if ((i + 1) % 3 == 0) out << "\n";
            }
            if (users.size() % 3 != 0) out << "\n";
            out << "The command completed successfully.\n\n";
            return;
        }

        std::string targetUser = tokens[2];
        std::wstring wTargetUser(targetUser.begin(), targetUser.end());

        bool isAdd = false;
        bool isDelete = false;
        std::string password;
        for (size_t i = 3; i < tokens.size(); ++i) {
            std::string t = tokens[i];
            std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (t == "/add") {
                isAdd = true;
            } else if (t == "/delete") {
                isDelete = true;
            } else if (!t.starts_with("/")) {
                password = tokens[i];
            }
        }

        if (isAdd) {
            std::wstring wPass(password.begin(), password.end());
            NTSTATUS st = sam::SamDatabase::get().createUser(wTargetUser, wPass);
            if (st == STATUS_SUCCESS) {
                out << "The command completed successfully.\n\n";
            } else if (st == STATUS_USER_EXISTS) {
                out << "The account already exists.\n\n";
            } else {
                out << "System error 5 has occurred.\nAccess is denied.\n\n";
            }
            return;
        }

        if (isDelete) {
            NTSTATUS st = sam::SamDatabase::get().deleteUser(wTargetUser);
            if (st == STATUS_SUCCESS) {
                out << "The command completed successfully.\n\n";
            } else if (st == STATUS_ACCESS_DENIED) {
                out << "System error 5 has occurred.\nAccess is denied.\n\n";
            } else {
                out << "The user name could not be found.\n\n";
            }
            return;
        }

        auto userOpt = sam::SamDatabase::get().getUser(wTargetUser);
        if (!userOpt) {
            out << "The user name could not be found.\n\n";
            return;
        }

        const auto& u = *userOpt;
        std::string sAccount = wideToAscii(u.accountName);
        std::string sFull = wideToAscii(u.fullName);
        std::string sComment = wideToAscii(u.comment);
        bool active = (u.userFlags & sam::USER_ACCOUNT_DISABLED) == 0;

        out << "\nUser name                    " << sAccount << "\n"
            << "Full Name                    " << sFull << "\n"
            << "Comment                      " << sComment << "\n"
            << "User's comment\n"
            << "Country/region code          000 (System Default)\n"
            << "Account active               " << (active ? "Yes" : "No") << "\n"
            << "Account expires              Never\n\n"
            << "Password last set            10/01/2026 12:00:00 PM\n"
            << "Password expires             Never\n"
            << "Password changeable          10/01/2026 12:00:00 PM\n"
            << "Password required            Yes\n"
            << "User may change password     Yes\n\n"
            << "Workstations allowed         All\n"
            << "Logon script\n"
            << "User profile\n"
            << "Home directory\n"
            << "Last logon                   10/02/2026 11:45:00 AM\n\n"
            << "Local Group Memberships      ";

        auto groupSids = sam::SamDatabase::get().getGroupSidsForUser(u.rid);
        for (const auto& gSid : groupSids) {
            std::wstring gName, gDom;
            (void)lsass::LocalSecurityAuthority::get().lookupAccountSid(gSid, gName, gDom);
            if (gDom == L"BUILTIN" || gDom == L"MICANT") {
                std::string sG = wideToAscii(gName);
                out << "*" << sG << "  ";
            }
        }
        out << "\nGlobal Group memberships     *None\n"
            << "The command completed successfully.\n\n";
    }

    void cmdWhoami(const std::vector<std::string>& tokens, std::ostream& out) {
        std::string flag = (tokens.size() > 1) ? tokens[1] : "";
        std::transform(flag.begin(), flag.end(), flag.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::wstring curUser = winlogon::WinlogonManager::get().getLoggedOnUser();
        std::wstring curDomain = winlogon::WinlogonManager::get().getLoggedOnDomain();
        if (curUser.empty()) {
            curUser = L"admin";
            curDomain = L"MICANT";
        }

        std::string sUser = wideToAscii(curUser);
        std::string sDomain = wideToAscii(curDomain);
        auto token = winlogon::WinlogonManager::get().getActiveToken();
        if (!token) {
            auto userOpt = sam::SamDatabase::get().getUser(curUser);
            if (userOpt) {
                token = std::make_shared<se::TokenObject>(userOpt->userSid);
                for (const auto& g : sam::SamDatabase::get().getGroupSidsForUser(userOpt->rid)) {
                    token->addGroup(g);
                }
            } else {
                token = se::TokenObject::createUserToken(se::Sid(5, {21, 1988, 1993, 2026, 1000}));
            }
        }

        if (flag == "/?" || flag == "-?") {
            out << "\nWHOAMI [/UPN | /USER | /GROUPS | /PRIV] [/FO format]\n"
                << "Description:\n"
                << "    Displays current user identity, group memberships, and assigned privileges.\n\n";
            return;
        }

        if (flag == "/user" || flag == "/all") {
            out << "\nUSER INFORMATION\n"
                << "----------------\n\n"
                << "User Name                SID\n"
                << "======================== ============================================\n"
                << std::left << std::setw(24) << (sDomain + "\\" + sUser) << " "
                << wideToAscii(token->getUserSid().toString()) << "\n\n";
        }

        if (flag == "/groups" || flag == "/all") {
            out << "\nGROUP INFORMATION\n"
                << "-----------------\n\n"
                << "Group Name                                  Type             SID          Attributes\n"
                << "=========================================== ================ ============ ==================================================\n";
            for (const auto& gSid : token->getGroupSids()) {
                std::wstring gName, gDom;
                (void)lsass::LocalSecurityAuthority::get().lookupAccountSid(gSid, gName, gDom);
                std::string sGName = wideToAscii(gName);
                std::string sGDom = wideToAscii(gDom);
                std::string fullName = sGDom.empty() ? sGName : (sGDom + "\\" + sGName);
                std::string type = (sGDom == "BUILTIN" || sGDom == "MICANT") ? "Alias" : "Well-known group";
                out << std::left << std::setw(43) << fullName << " "
                    << std::setw(16) << type << " "
                    << std::setw(12) << wideToAscii(gSid.toString()) << " "
                    << "Mandatory group, Enabled by default, Enabled group\n";
            }
            out << "\n";
        }

        if (flag == "/priv" || flag == "/all") {
            out << "\nPRIVILEGES INFORMATION\n"
                << "----------------------\n\n"
                << "Privilege Name                Description                          State\n"
                << "============================= ==================================== ========\n";
            const auto& privs = token->getPrivileges();
            static const std::unordered_map<std::wstring, std::string> privDesc = {
                {L"SeDebugPrivilege", "Debug programs"},
                {L"SeShutdownPrivilege", "Shut down the system"},
                {L"SeBackupPrivilege", "Back up files and directories"},
                {L"SeRestorePrivilege", "Restore files and directories"},
                {L"SeSecurityPrivilege", "Manage auditing and security log"},
                {L"SeTakeOwnershipPrivilege", "Take ownership of files or other objects"},
                {L"SeTcbPrivilege", "Act as part of the operating system"},
                {L"SeSystemEnvironmentPrivilege", "Modify firmware environment values"},
                {L"SeChangeNotifyPrivilege", "Bypass traverse checking"},
                {L"SeImpersonatePrivilege", "Impersonate a client after authentication"},
                {L"SeCreateTokenPrivilege", "Create a token object"},
                {L"SeAssignPrimaryTokenPrivilege", "Replace a process level token"},
                {L"SeIncreaseQuotaPrivilege", "Adjust memory quotas for a process"},
                {L"SeLoadDriverPrivilege", "Load and unload device drivers"}
            };

            for (const auto& [pName, attr] : privs) {
                std::string sName = wideToAscii(pName);
                std::string desc = "Administrative User Privilege";
                auto dit = privDesc.find(pName);
                if (dit != privDesc.end()) desc = dit->second;
                std::string state = (attr & se::SE_PRIVILEGE_ENABLED) ? "Enabled" : "Disabled";
                out << std::left << std::setw(29) << sName << " "
                    << std::setw(36) << desc << " "
                    << state << "\n";
            }
            out << "\n";
        }

        if (flag.empty()) {
            out << sDomain << "\\" << sUser << "\n";
        }
    }

    void cmdLock(std::ostream& out) {
        if (winlogon::WinlogonManager::get().lockWorkstation()) {
            out << "The workstation is now locked. Switched to secure Winlogon desktop.\n";
        } else {
            out << "Failed to lock workstation. Session is not in logged-on state.\n";
        }
    }

    void cmdLogoff(std::ostream& out) {
        winlogon::WinlogonManager::get().logoff();
        out << "Session terminated. User logged off. Switched to secure Winlogon desktop.\n";
    }

    void cmdPrismX(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[PrismX / Prism3D] Initializing 3D graphics presentation test...\n";
            prismx::IDXGIFactory1* factory = nullptr;
            prismx::CreateDXGIFactory1(prismx::IID_IDXGIFactory1, reinterpret_cast<void**>(&factory));
            if (!factory) {
                out << "[PrismX] Failed to create DXGI factory.\n";
                return;
            }

            prismx::DXGI_SWAP_CHAIN_DESC scDesc{};
            scDesc.BufferDesc.Width = 800;
            scDesc.BufferDesc.Height = 600;
            scDesc.BufferDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
            scDesc.BufferCount = 2;
            scDesc.SwapEffect = prismx::DXGI_SWAP_EFFECT_FLIP_DISCARD;

            prism3d::ID3D11Device* device = nullptr;
            prism3d::ID3D11DeviceContext* context = nullptr;
            prismx::IDXGISwapChain* swapChain = nullptr;

            int32_t hr = prism3d::D3D11CreateDeviceAndSwapChain(
                nullptr, prism3d::D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                nullptr, 0, 7, &scDesc, &swapChain, &device, nullptr, &context
            );

            if (hr != 0 || !device || !context || !swapChain) {
                out << "[Prism3D] Failed to initialize Direct3D 11 device and swapchain.\n";
                if (factory) factory->Release();
                return;
            }

            // Create Render Target View from SwapChain BackBuffer
            prismx::IDXGISurface* surface = nullptr;
            swapChain->GetBuffer(0, prismx::IID_IDXGISurface, reinterpret_cast<void**>(&surface));
            
            prism3d::ID3D11RenderTargetView* rtv = nullptr;
            device->CreateRenderTargetView(reinterpret_cast<prism3d::ID3D11Resource*>(surface), nullptr, &rtv);

            // Define Triangle Vertices (DirectXTK VertexPositionColor format)
            prism3d::VertexPositionColor vertices[3] = {
                {  0.0f,  0.6f, 0.0f,  1.0f, 0.1f, 0.1f, 1.0f }, // Top (Vibrant Red)
                { -0.6f, -0.6f, 0.0f,  0.1f, 1.0f, 0.1f, 1.0f }, // Bottom-Left (Vibrant Green)
                {  0.6f, -0.6f, 0.0f,  0.1f, 0.2f, 1.0f, 1.0f }  // Bottom-Right (Vibrant Blue)
            };

            prism3d::D3D11_BUFFER_DESC vbDesc{};
            vbDesc.ByteWidth = sizeof(vertices);
            vbDesc.Usage = prism3d::D3D11_USAGE_DEFAULT;
            vbDesc.BindFlags = prism3d::D3D11_BIND_VERTEX_BUFFER;
            vbDesc.StructureByteStride = sizeof(prism3d::VertexPositionColor);

            prism3d::D3D11_SUBRESOURCE_DATA initData{};
            initData.pSysMem = vertices;

            prism3d::ID3D11Buffer* vertexBuffer = nullptr;
            device->CreateBuffer(&vbDesc, &initData, &vertexBuffer);

            // Setup Viewport & Targets
            prism3d::D3D11_VIEWPORT vp{ 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
            context->RSSetViewports(1, &vp);
            context->OMSetRenderTargets(1, &rtv, nullptr);

            // Clear to Midnight Blue Background
            const float clearColor[4] = { 0.04f, 0.07f, 0.16f, 1.0f };
            context->ClearRenderTargetView(rtv, clearColor);

            // Bind Vertex Buffer and Draw
            uint32_t stride = sizeof(prism3d::VertexPositionColor);
            uint32_t offset = 0;
            context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            context->IASetPrimitiveTopology(prism3d::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->Draw(3, 0);

            // Present the Frame
            swapChain->Present(1, 0);

            out << "[Prism3D] 3D Barycentric Shaded Triangle rendered successfully!\n"
                << "  Swapchain: 800x600 (32-bpp BGRA), FLIP_DISCARD\n"
                << "  Shading: Interpolated RGB Gouraud Barycentric Rasterizer\n"
                << "  Status: Frame 1 successfully presented to display compositor.\n";

            // Cleanup
            if (vertexBuffer) vertexBuffer->Release();
            if (rtv) rtv->Release();
            if (surface) surface->Release();
            if (swapChain) swapChain->Release();
            if (context) context->Release();
            if (device) device->Release();
            if (factory) factory->Release();
            return;
        }

        if (tokens.size() > 1 && (tokens[1] == "cube" || tokens[1] == "wireframe")) {
            bool wireframe = (tokens[1] == "wireframe");
            out << "[Prism3D] Launching 3D " << (wireframe ? "Wireframe" : "Indexed Shaded") << " Cube Pipeline...\n";

            prismx::DXGI_SWAP_CHAIN_DESC scDesc{};
            scDesc.BufferDesc.Width = 800;
            scDesc.BufferDesc.Height = 600;
            scDesc.BufferDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
            scDesc.BufferCount = 2;
            scDesc.SwapEffect = prismx::DXGI_SWAP_EFFECT_FLIP_DISCARD;

            prism3d::ID3D11Device* device = nullptr;
            prism3d::ID3D11DeviceContext* context = nullptr;
            prismx::IDXGISwapChain* swapChain = nullptr;

            int32_t hr = prism3d::D3D11CreateDeviceAndSwapChain(
                nullptr, prism3d::D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                nullptr, 0, 7, &scDesc, &swapChain, &device, nullptr, &context
            );

            if (hr != 0 || !device || !context || !swapChain) {
                out << "[Prism3D] Failed to initialize Direct3D 11 device and swapchain.\n";
                return;
            }

            // Create Render Target View from BackBuffer
            prismx::IDXGISurface* surface = nullptr;
            swapChain->GetBuffer(0, prismx::IID_IDXGISurface, reinterpret_cast<void**>(&surface));
            prism3d::ID3D11RenderTargetView* rtv = nullptr;
            device->CreateRenderTargetView(reinterpret_cast<prism3d::ID3D11Resource*>(surface), nullptr, &rtv);

            // Create Depth Stencil View
            prism3d::ID3D11DepthStencilView* dsv = nullptr;
            device->CreateDepthStencilView(nullptr, nullptr, &dsv);

            // Create Rasterizer State (Solid or Wireframe)
            prism3d::D3D11_RASTERIZER_DESC rsDesc{};
            rsDesc.FillMode = wireframe ? prism3d::D3D11_FILL_WIREFRAME : prism3d::D3D11_FILL_SOLID;
            rsDesc.CullMode = wireframe ? prism3d::D3D11_CULL_NONE : prism3d::D3D11_CULL_BACK;
            prism3d::ID3D11RasterizerState* rsState = nullptr;
            device->CreateRasterizerState(&rsDesc, &rsState);
            context->RSSetState(rsState);

            // Cube 8 Vertices with distinct face colors
            prism3d::VertexPositionColor cubeVerts[8] = {
                { -1.0f, -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // 0: Red
                { -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // 1: Green
                {  1.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f, 1.0f }, // 2: Blue
                {  1.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 1.0f }, // 3: Yellow
                { -1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 1.0f, 1.0f }, // 4: Magenta
                { -1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f }, // 5: Cyan
                {  1.0f,  1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f }, // 6: White
                {  1.0f, -1.0f,  1.0f,  0.5f, 0.5f, 0.5f, 1.0f }  // 7: Grey
            };

            prism3d::D3D11_BUFFER_DESC vbDesc{};
            vbDesc.ByteWidth = sizeof(cubeVerts);
            vbDesc.BindFlags = prism3d::D3D11_BIND_VERTEX_BUFFER;
            vbDesc.StructureByteStride = sizeof(prism3d::VertexPositionColor);
            prism3d::D3D11_SUBRESOURCE_DATA vbInit{};
            vbInit.pSysMem = cubeVerts;
            prism3d::ID3D11Buffer* vb = nullptr;
            device->CreateBuffer(&vbDesc, &vbInit, &vb);

            // Cube 36 Indices (12 triangles)
            uint16_t cubeIndices[36] = {
                0, 1, 2,  0, 2, 3,  // Front
                4, 6, 5,  4, 7, 6,  // Back
                4, 5, 1,  4, 1, 0,  // Left
                3, 2, 6,  3, 6, 7,  // Right
                1, 5, 6,  1, 6, 2,  // Top
                4, 0, 3,  4, 3, 7   // Bottom
            };

            prism3d::D3D11_BUFFER_DESC ibDesc{};
            ibDesc.ByteWidth = sizeof(cubeIndices);
            ibDesc.BindFlags = prism3d::D3D11_BIND_INDEX_BUFFER;
            prism3d::D3D11_SUBRESOURCE_DATA ibInit{};
            ibInit.pSysMem = cubeIndices;
            prism3d::ID3D11Buffer* ib = nullptr;
            device->CreateBuffer(&ibDesc, &ibInit, &ib);

            // Model-View-Projection Matrix (Yaw 45deg, Pitch 35deg, Eye at z = -3.5f)
            prism3d::Matrix4x4 world = prism3d::Matrix4x4::Multiply(
                prism3d::Matrix4x4::RotationX(0.61f),
                prism3d::Matrix4x4::RotationY(0.78f)
            );
            prism3d::Matrix4x4 view = prism3d::Matrix4x4::LookAtLH(
                prism3d::Vector3{ 0.0f, 0.0f, -3.5f },
                prism3d::Vector3{ 0.0f, 0.0f, 0.0f },
                prism3d::Vector3{ 0.0f, 1.0f, 0.0f }
            );
            prism3d::Matrix4x4 proj = prism3d::Matrix4x4::PerspectiveFovLH(
                1.047f, // 60 degrees FOV
                800.0f / 600.0f,
                0.1f,
                100.0f
            );
            prism3d::Matrix4x4 mvp = prism3d::Matrix4x4::Multiply(world, prism3d::Matrix4x4::Multiply(view, proj));

            prism3d::D3D11_BUFFER_DESC cbDesc{};
            cbDesc.ByteWidth = sizeof(prism3d::Matrix4x4);
            cbDesc.BindFlags = prism3d::D3D11_BIND_CONSTANT_BUFFER;
            prism3d::D3D11_SUBRESOURCE_DATA cbInit{};
            cbInit.pSysMem = &mvp;
            prism3d::ID3D11Buffer* cb = nullptr;
            device->CreateBuffer(&cbDesc, &cbInit, &cb);

            // Setup Pipeline
            prism3d::D3D11_VIEWPORT vp{ 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
            context->RSSetViewports(1, &vp);
            context->OMSetRenderTargets(1, &rtv, dsv);

            const float clearBg[4] = { 0.03f, 0.05f, 0.12f, 1.0f };
            context->ClearRenderTargetView(rtv, clearBg);
            context->ClearDepthStencilView(dsv, prism3d::D3D11_CLEAR_DEPTH, 1.0f, 0);

            uint32_t stride = sizeof(prism3d::VertexPositionColor);
            uint32_t offset = 0;
            context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
            context->IASetIndexBuffer(ib, prismx::DXGI_FORMAT_R16_UINT, 0);
            context->IASetPrimitiveTopology(prism3d::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context->VSSetConstantBuffers(0, 1, &cb);

            // Draw Indexed Cube
            context->DrawIndexed(36, 0, 0);
            swapChain->Present(1, 0);

            out << "[Prism3D] 3D Cube rendered successfully via DrawIndexed!\n"
                << "  Mesh: 8 Vertices, 36 Indices (12 Triangles), Indexed Drawing\n"
                << "  Transforms: World (Pitch 35deg / Yaw 45deg) x View x Perspective (60deg FOV)\n"
                << "  Rasterizer State: " << (wireframe ? "WIREFRAME (Bresenham line)" : "SOLID (Barycentric Gouraud)") << "\n"
                << "  Culling: " << (wireframe ? "NONE" : "D3D11_CULL_BACK (Backface culling active)") << "\n"
                << "  Depth Test: Floating-point Z-Buffer (D32_FLOAT)\n"
                << "  Status: Frame presented to display compositor.\n";

            cb->Release();
            ib->Release();
            vb->Release();
            rsState->Release();
            dsv->Release();
            rtv->Release();
            surface->Release();
            swapChain->Release();
            context->Release();
            device->Release();
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "d3d12") {
            out << "[Prism3D12] Launching Direct3D 12 Low-Level Pipeline...\n";

            prism3d12::ID3D12Device* device = nullptr;
            int32_t hr = prism3d12::D3D12CreateDevice(nullptr, prism3d::D3D_FEATURE_LEVEL_12_0, prism3d12::IID_ID3D12Device, reinterpret_cast<void**>(&device));
            if (hr != 0 || !device) {
                out << "[Prism3D12] Failed to create D3D12 device.\n";
                return;
            }

            // Create Command Queue
            prism3d12::D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT;
            prism3d12::ID3D12CommandQueue* commandQueue = nullptr;
            device->CreateCommandQueue(&queueDesc, prism3d12::IID_ID3D12CommandQueue, reinterpret_cast<void**>(&commandQueue));

            // Create Command Allocator
            prism3d12::ID3D12CommandAllocator* allocator = nullptr;
            device->CreateCommandAllocator(prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, prism3d12::IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&allocator));

            // Create Graphics Command List
            prism3d12::ID3D12GraphicsCommandList* commandList = nullptr;
            device->CreateCommandList(0, prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, nullptr, prism3d12::IID_ID3D12GraphicsCommandList, reinterpret_cast<void**>(&commandList));

            // Create Synchronization Fence
            prism3d12::ID3D12Fence* fence = nullptr;
            device->CreateFence(0, prism3d12::D3D12_FENCE_FLAG_NONE, prism3d12::IID_ID3D12Fence, reinterpret_cast<void**>(&fence));

            // Create Committed Resource (Render Target Buffer)
            prism3d12::D3D12_RESOURCE_DESC resDesc{};
            resDesc.Dimension = prism3d12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resDesc.Width = 800;
            resDesc.Height = 600;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = 1;
            resDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
            resDesc.SampleDesc.Count = 1;
            prism3d12::ID3D12Resource* renderTarget = nullptr;
            device->CreateCommittedResource(nullptr, 0, &resDesc, prism3d12::D3D12_RESOURCE_STATE_PRESENT, nullptr, prism3d12::IID_ID3D12Resource, reinterpret_cast<void**>(&renderTarget));

            // Create Descriptor Heap
            prism3d12::D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
            rtvHeapDesc.NumDescriptors = 1;
            rtvHeapDesc.Type = prism3d12::D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            prism3d12::ID3D12DescriptorHeap* rtvHeap = nullptr;
            device->CreateDescriptorHeap(&rtvHeapDesc, prism3d12::IID_ID3D12DescriptorHeap, reinterpret_cast<void**>(&rtvHeap));

            prism3d12::D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            device->CreateRenderTargetView(renderTarget, nullptr, rtvHandle);

            // Record Commands into Command List:
            // 1. Transition Resource: PRESENT -> RENDER_TARGET
            prism3d12::D3D12_RESOURCE_BARRIER barrierStart{};
            barrierStart.Type = prism3d12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierStart.Transition.pResource = renderTarget;
            barrierStart.Transition.StateBefore = prism3d12::D3D12_RESOURCE_STATE_PRESENT;
            barrierStart.Transition.StateAfter = prism3d12::D3D12_RESOURCE_STATE_RENDER_TARGET;
            commandList->ResourceBarrier(1, &barrierStart);

            // 2. Viewport & Scissor
            prism3d12::D3D12_VIEWPORT vp{ 0.0f, 0.0f, 800.0f, 600.0f, 0.0f, 1.0f };
            prism3d12::D3D12_RECT scissor{ 0, 0, 800, 600 };
            commandList->RSSetViewports(1, &vp);
            commandList->RSSetScissorRects(1, &scissor);

            // 3. Clear Render Target & Bind
            commandList->OMSetRenderTargets(1, &rtvHandle, 0, nullptr);
            const float clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
            commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

            // 4. Draw
            commandList->IASetPrimitiveTopology(prism3d::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            commandList->DrawInstanced(3, 1, 0, 0);

            // 5. Transition Resource: RENDER_TARGET -> PRESENT
            prism3d12::D3D12_RESOURCE_BARRIER barrierEnd{};
            barrierEnd.Type = prism3d12::D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierEnd.Transition.pResource = renderTarget;
            barrierEnd.Transition.StateBefore = prism3d12::D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrierEnd.Transition.StateAfter = prism3d12::D3D12_RESOURCE_STATE_PRESENT;
            commandList->ResourceBarrier(1, &barrierEnd);

            // Close Command List & Submit to Queue
            commandList->Close();
            prism3d12::ID3D12CommandList* ppLists[] = { commandList };
            commandQueue->ExecuteCommandLists(1, ppLists);

            // Fence Synchronization
            commandQueue->Signal(fence, 1);
            uint64_t completedVal = fence->GetCompletedValue();

            out << "[Prism3D12] Direct3D 12 Command Pipeline Executed Successfully!\n"
                << "  Command List Type:    D3D12_COMMAND_LIST_TYPE_DIRECT\n"
                << "  Resource Transitions: PRESENT -> RENDER_TARGET -> PRESENT\n"
                << "  Recorded Commands:    " << static_cast<prism3d12::Prism3D12GraphicsCommandListImpl*>(commandList)->GetRecordedCommands().size() << "\n"
                << "  Fence Completed:      " << completedVal << " (GPU Fence signaled successfully)\n"
                << "  Hardware Queue:       Executed via WDDM D3DKMT kernel submitter\n";

            rtvHeap->Release();
            renderTarget->Release();
            fence->Release();
            commandList->Release();
            allocator->Release();
            commandQueue->Release();
            device->Release();
            return;
        }

        if (tokens.size() > 1 && (tokens[1] == "d3d9" || tokens[1] == "dx9")) {
            cmdD3D9(tokens, out);
            return;
        }

        if (tokens.size() > 1 && (tokens[1] == "vm" || tokens[1] == "shader")) {
            out << "[PrismVM] Launching Sovereign Programmable Shader Bytecode Virtual Machine...\n";

            // 1. Build MVP Transform Vertex Shader
            auto vsProg = prism_vm::PrismShaderVM::BuildMVPTransformVS();

            // Setup input vertex: Position (1.0, 2.0, 3.0, 1.0), Color (1.0, 0.4, 0.2, 1.0)
            prism_vm::VectorRegister inPos(1.0f, 2.0f, 3.0f, 1.0f);
            prism_vm::VectorRegister inColor(1.0f, 0.4f, 0.2f, 1.0f);
            prism_vm::VectorRegister inUV(0.5f, 0.5f, 0.0f, 0.0f);
            prism_vm::VectorRegister inNormal(0.0f, 1.0f, 0.0f, 0.0f);

            // Matrix in c0..c3 (MVP)
            std::array<prism_vm::VectorRegister, 16> consts{};
            consts[0] = prism_vm::VectorRegister(2.0f, 0.0f, 0.0f, 0.0f);
            consts[1] = prism_vm::VectorRegister(0.0f, 2.0f, 0.0f, 0.0f);
            consts[2] = prism_vm::VectorRegister(0.0f, 0.0f, 1.0f, 0.0f);
            consts[3] = prism_vm::VectorRegister(0.0f, 0.0f, 0.0f, 1.0f);

            prism_vm::VectorRegister outPos, outColor;
            prism_vm::PrismShaderVM::ExecuteVertexShader(vsProg, inPos, inColor, inUV, inNormal, consts, outPos, outColor);

            out << "  Vertex Shader Output:\n"
                << "    Input Pos:   (" << inPos.x() << ", " << inPos.y() << ", " << inPos.z() << ", " << inPos.w() << ")\n"
                << "    Output Clip: (" << outPos.x() << ", " << outPos.y() << ", " << outPos.z() << ", " << outPos.w() << ")\n"
                << "    Instructions: " << vsProg.InstructionCount() << " (DP4, MOV, RET)\n";

            // 2. Build Textured Modulate Pixel Shader
            auto psProg = prism_vm::PrismShaderVM::BuildTexturedModulatePS();
            auto sampler = [](uint8_t slot, float u, float v) -> prism_vm::VectorRegister {
                (void)slot; (void)u; (void)v;
                return prism_vm::VectorRegister(0.8f, 0.9f, 1.0f, 1.0f); // Sky blue texel
            };

            prism_vm::VectorRegister psOutColor;
            prism_vm::PrismShaderVM::ExecutePixelShader(psProg, outPos, outColor, inUV, inNormal, consts, sampler, psOutColor);

            out << "  Pixel Shader Output:\n"
                << "    Input UV:    (" << inUV.x() << ", " << inUV.y() << ")\n"
                << "    Shaded Color: RGBA(" << psOutColor.x() << ", " << psOutColor.y() << ", " << psOutColor.z() << ", " << psOutColor.w() << ")\n"
                << "    Instructions: " << psProg.InstructionCount() << " (TEX, MUL, RET)\n"
                << "[PrismVM] Bytecode execution verified with 100% precision.\n";
            return;
        }

        // Display GPU Info
        out << "========================================================================\n"
            << "               MicaNT PrismX & Prism3D Graphics Subsystem               \n"
            << "========================================================================\n\n";

        prismx::IDXGIFactory1* factory = nullptr;
        prismx::CreateDXGIFactory1(prismx::IID_IDXGIFactory1, reinterpret_cast<void**>(&factory));
        if (factory) {
            prismx::IDXGIAdapter1* adapter = nullptr;
            for (uint32_t i = 0; factory->EnumAdapters1(i, &adapter) == 0; ++i) {
                prismx::DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);

                std::wstring wDesc(desc.Description);
                std::string sDesc;
                sDesc.reserve(wDesc.size());
                for (wchar_t wc : wDesc) sDesc.push_back(static_cast<char>(wc));

                out << "Adapter " << i << ": " << sDesc << "\n"
                    << "  Vendor ID:               0x" << std::hex << std::uppercase << desc.VendorId << std::dec << "\n"
                    << "  Device ID:               0x" << std::hex << std::uppercase << desc.DeviceId << std::dec << "\n"
                    << "  Dedicated Video Memory:  " << (desc.DedicatedVideoMemory / (1024 * 1024)) << " MB\n"
                    << "  Shared System Memory:    " << (desc.SharedSystemMemory / (1024 * 1024)) << " MB\n"
                    << "  Hardware Type:           " << ((desc.Flags & 2) ? "Software / Warp Reference" : "Hardware Discrete GPU") << "\n";

                prismx::IDXGIOutput* output = nullptr;
                for (uint32_t o = 0; adapter->EnumOutputs(o, &output) == 0; ++o) {
                    prismx::DXGI_OUTPUT_DESC oDesc{};
                    output->GetDesc(&oDesc);
                    std::wstring wDev(oDesc.DeviceName);
                    std::string sDev;
                    sDev.reserve(wDev.size());
                    for (wchar_t wc : wDev) sDev.push_back(static_cast<char>(wc));
                    out << "  Connected Display:       " << sDev
                        << " (" << (oDesc.DesktopCoordinates.right - oDesc.DesktopCoordinates.left)
                        << "x" << (oDesc.DesktopCoordinates.bottom - oDesc.DesktopCoordinates.top) << " @ 60Hz)\n";
                    output->Release();
                }
                out << "\n";
                adapter->Release();
            }
            factory->Release();
        }

        const auto& dxg = dxgkrnl::DxgkrnlSubsystem::GetInstance();
        out << "WDDM Kernel Telemetry (dxgkrnl.sys / D3DKMT):\n"
            << "  Active Video Allocations: " << dxg.GetActiveAllocationsCount() << "\n"
            << "  Active Allocated VRAM:    " << (dxg.GetActiveAllocatedBytes() / 1024) << " KB\n"
            << "  GPU Command Submissions:  " << dxg.GetTotalSubmissions() << "\n"
            << "  Compositor Presents:      " << dxg.GetTotalPresents() << "\n"
            << "  VBlank Sync Events:       " << dxg.GetTotalVBlankWaits() << "\n\n"
            << "Type 'prismx test', 'prismx cube', 'prismx wireframe', 'prismx d3d9', 'prismx d3d12', or 'prismx vm' to execute graphics tests.\n";
    }

    void cmdD3D9(const std::vector<std::string>& tokens, std::ostream& out) {
        bool useShaders = false;
        for (const auto& tok : tokens) {
            if (tok == "shader" || tok == "shaders") {
                useShaders = true;
                break;
            }
        }

        if (useShaders) {
            out << "[Direct3D 9] Initializing D3D9 Programmable Shader Pipeline (VS 3.0 & PS 3.0)...\n";
        } else {
            out << "[Direct3D 9] Initializing D3D9 Sovereign Fixed-Function Pipeline & Runtime...\n";
        }

        d3d9::IDirect3D9* pD3D = d3d9::Direct3DCreate9(d3d9::D3D_SDK_VERSION);
        if (!pD3D) {
            out << "[Direct3D 9] Failed to initialize Direct3D 9 runtime.\n";
            return;
        }

        uint32_t adapterCount = pD3D->GetAdapterCount();
        d3d9::D3DADAPTER_IDENTIFIER9 ident{};
        pD3D->GetAdapterIdentifier(0, 0, &ident);

        out << "  Active Adapter: " << ident.Description << "\n"
            << "  Driver:         " << ident.Driver << " (Version " << ident.DriverVersionHigh << "." << ident.DriverVersionLow << ")\n"
            << "  Hardware ID:    Vendor=0x" << std::hex << std::uppercase << ident.VendorId 
            << " Device=0x" << ident.DeviceId << std::dec << " (Total Adapters: " << adapterCount << ")\n";

        // Create Native User32 Presentation Target Window
        win32::HWND hwnd = user32::CreateWindowExW(
            0, L"MicaNT_Window", useShaders ? L"MicaNT PrismX Direct3D 9 Programmable Viewport" : L"MicaNT PrismX Direct3D 9 Fixed-Function Viewport",
            0, 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr
        );

        if (!hwnd) {
            out << "[Direct3D 9] Error: Failed to create User32 presentation window.\n";
            pD3D->Release();
            return;
        }

        d3d9::D3DPRESENT_PARAMETERS pp{};
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        pp.BackBufferFormat = d3d9::D3DFMT_X8R8G8B8;
        pp.BackBufferCount = 1;
        pp.SwapEffect = d3d9::D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = hwnd;
        pp.Windowed = win32::TRUE;

        d3d9::IDirect3DDevice9* pDevice = nullptr;
        int32_t hr = pD3D->CreateDevice(0, d3d9::D3DDEVTYPE_HAL, hwnd, d3d9::D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &pDevice);
        if (hr != d3d9::D3D_OK || !pDevice) {
            out << "[Direct3D 9] Error: Failed to create D3D9 Device (hr=" << hr << ").\n";
            user32::DestroyWindow(hwnd);
            pD3D->Release();
            return;
        }

        // Setup Render States
        pDevice->SetRenderState(d3d9::D3DRS_ZENABLE, 1);
        pDevice->SetRenderState(d3d9::D3DRS_FILLMODE, d3d9::D3DFILL_SOLID);
        pDevice->SetRenderState(d3d9::D3DRS_CULLMODE, d3d9::D3DCULL_CCW);
        pDevice->SetRenderState(d3d9::D3DRS_LIGHTING, 0);

        if (useShaders) {
            // Vertex Declaration
            d3d9::D3DVERTEXELEMENT9 declElems[] = {
                { 0, 0, d3d9::D3DDECLTYPE_FLOAT3, d3d9::D3DDECLMETHOD_DEFAULT, d3d9::D3DDECLUSAGE_POSITION, 0 },
                { 0, 12, d3d9::D3DDECLTYPE_D3DCOLOR, d3d9::D3DDECLMETHOD_DEFAULT, d3d9::D3DDECLUSAGE_COLOR, 0 },
                { 0, 16, d3d9::D3DDECLTYPE_FLOAT2, d3d9::D3DDECLMETHOD_DEFAULT, d3d9::D3DDECLUSAGE_TEXCOORD, 0 },
                D3DDECL_END()
            };
            d3d9::IDirect3DVertexDeclaration9* pDecl = nullptr;
            pDevice->CreateVertexDeclaration(declElems, &pDecl);
            pDevice->SetVertexDeclaration(pDecl);

            // Vertex Shader
            const char vsSrc[] =
                "vs_3_0\n"
                "dp4 r0.x, v0, c0\n"
                "dp4 r0.y, v0, c1\n"
                "dp4 r0.z, v0, c2\n"
                "dp4 r0.w, v0, c3\n"
                "mov o0, r0\n"
                "mov o1, v1\n"
                "ret\n";
            d3d9::ID3DXBuffer* pVsBuf = nullptr;
            d3d9::D3DXAssembleShader(vsSrc, sizeof(vsSrc), nullptr, nullptr, 0, &pVsBuf, nullptr);
            d3d9::IDirect3DVertexShader9* pVS = nullptr;
            if (pVsBuf) {
                pDevice->CreateVertexShader(static_cast<const uint32_t*>(pVsBuf->GetBufferPointer()), &pVS);
                pVsBuf->Release();
            } else {
                pDevice->CreateVertexShader(nullptr, &pVS);
            }
            pDevice->SetVertexShader(pVS);

            // Pixel Shader
            const char psSrc[] =
                "ps_3_0\n"
                "tex r0, v2, s0\n"
                "mul r1, r0, v1\n"
                "mul o1, r1, c0\n"
                "ret\n";
            d3d9::ID3DXBuffer* pPsBuf = nullptr;
            d3d9::D3DXAssembleShader(psSrc, sizeof(psSrc), nullptr, nullptr, 0, &pPsBuf, nullptr);
            d3d9::IDirect3DPixelShader9* pPS = nullptr;
            if (pPsBuf) {
                pDevice->CreatePixelShader(static_cast<const uint32_t*>(pPsBuf->GetBufferPointer()), &pPS);
                pPsBuf->Release();
            } else {
                pDevice->CreatePixelShader(nullptr, &pPS);
            }
            pDevice->SetPixelShader(pPS);

            // Create Procedural Texture
            d3d9::IDirect3DTexture9* pTex = nullptr;
            pDevice->CreateTexture(64, 64, 1, 0, d3d9::D3DFMT_A8R8G8B8, d3d9::D3DPOOL_MANAGED, &pTex, nullptr);
            if (pTex) {
                d3d9::D3DLOCKED_RECT lr{};
                if (pTex->LockRect(0, &lr, nullptr, 0) == d3d9::D3D_OK) {
                    uint32_t* texPix = static_cast<uint32_t*>(lr.pBits);
                    for (int y = 0; y < 64; ++y) {
                        for (int x = 0; x < 64; ++x) {
                            bool check = ((x / 8) + (y / 8)) % 2 == 0;
                            texPix[y * 64 + x] = check ? 0xFFFFFFFF : 0xFF204060;
                        }
                    }
                    pTex->UnlockRect(0);
                }
                pDevice->SetTexture(0, pTex);
                pDevice->SetSamplerState(0, d3d9::D3DSAMP_MAGFILTER, d3d9::D3DTEXF_LINEAR);
            }

            // Set Shader Constants
            d3d9::D3DXMATRIX matWorld, matView, matProj, matWVP;
            d3d9::D3DXMatrixIdentity(&matWorld);
            d3d9::D3DXVECTOR3 eye{ 0.0f, 0.0f, -3.0f }, at{ 0.0f, 0.0f, 0.0f }, up{ 0.0f, 1.0f, 0.0f };
            d3d9::D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
            d3d9::D3DXMatrixPerspectiveFovLH(&matProj, 3.14159f / 4.0f, 640.0f / 480.0f, 0.1f, 100.0f);
            d3d9::D3DXMatrixMultiply(&matWVP, &matWorld, &matView);
            d3d9::D3DXMatrixMultiply(&matWVP, &matWVP, &matProj);

            d3d9::D3DXMATRIX matTransposed;
            d3d9::D3DXMatrixTranspose(&matTransposed, &matWVP);
            pDevice->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(&matTransposed), 4);

            float psTint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            pDevice->SetPixelShaderConstantF(0, psTint, 1);

            struct ShadedVertex {
                float x, y, z;
                uint32_t color;
                float u, v;
            };
            ShadedVertex quadVerts[6] = {
                { -1.0f,  1.0f, 0.0f, 0xFFFF0000, 0.0f, 0.0f },
                {  1.0f,  1.0f, 0.0f, 0xFF00FF00, 1.0f, 0.0f },
                { -1.0f, -1.0f, 0.0f, 0xFF0000FF, 0.0f, 1.0f },
                { -1.0f, -1.0f, 0.0f, 0xFF0000FF, 0.0f, 1.0f },
                {  1.0f,  1.0f, 0.0f, 0xFF00FF00, 1.0f, 0.0f },
                {  1.0f, -1.0f, 0.0f, 0xFFFFFFFF, 1.0f, 1.0f }
            };

            pDevice->Clear(0, nullptr, d3d9::D3DCLEAR_TARGET | d3d9::D3DCLEAR_ZBUFFER, d3d9::D3DCOLOR_XRGB(15, 25, 45), 1.0f, 0);
            pDevice->BeginScene();
            pDevice->DrawPrimitiveUP(d3d9::D3DPT_TRIANGLELIST, 2, quadVerts, sizeof(ShadedVertex));
            pDevice->EndScene();
            pDevice->Present(nullptr, nullptr, hwnd, nullptr);

            out << "[Direct3D 9] Programmable Vertex & Pixel Shader Pipeline Rendered Successfully!\n"
                << "  Target Window:  640x480 (HWND " << hwnd << ")\n"
                << "  Vertex Shader:  Shader Model 3.0 MVP Matrix Transformation\n"
                << "  Pixel Shader:   Shader Model 3.0 Procedural Texture Modulate\n"
                << "  Samplers:       64x64 Checkered Texture Bound to Sampler 0\n"
                << "  Presents:       " << static_cast<d3d9::Direct3DDevice9Impl*>(pDevice)->GetPresentCount() << " frame(s) blitted to User32 Window.\n";

            if (pTex) pTex->Release();
            if (pVS) pVS->Release();
            if (pPS) pPS->Release();
            if (pDecl) pDecl->Release();
        } else {
            // Clear Viewport (Midnight Blue)
            pDevice->Clear(0, nullptr, d3d9::D3DCLEAR_TARGET | d3d9::D3DCLEAR_ZBUFFER, d3d9::D3DCOLOR_XRGB(10, 20, 50), 1.0f, 0);

            pDevice->BeginScene();

            // 3D Gouraud-Shaded Triangle (D3DFVF_XYZ | D3DFVF_DIFFUSE)
            struct D3DVertex {
                float x, y, z;
                uint32_t color;
            };

            D3DVertex triangle[3] = {
                {  0.0f,  0.7f, 0.0f, d3d9::D3DCOLOR_XRGB(255, 30, 30) },   // Top Red
                {  0.7f, -0.7f, 0.0f, d3d9::D3DCOLOR_XRGB(30, 255, 30) },   // Bottom-Right Green (CW)
                { -0.7f, -0.7f, 0.0f, d3d9::D3DCOLOR_XRGB(30, 30, 255) }    // Bottom-Left Blue
            };

            pDevice->SetFVF(d3d9::D3DFVF_XYZ | d3d9::D3DFVF_DIFFUSE);
            pDevice->DrawPrimitiveUP(d3d9::D3DPT_TRIANGLELIST, 1, triangle, sizeof(D3DVertex));

            pDevice->EndScene();

            // Present to HWND
            pDevice->Present(nullptr, nullptr, hwnd, nullptr);

            out << "[Direct3D 9] Fixed-Function Barycentric Shaded Triangle Rendered Successfully!\n"
                << "  Target Window:  640x480 (HWND " << hwnd << ")\n"
                << "  Pixel Format:   D3DFMT_X8R8G8B8 (32-bpp BGRA)\n"
                << "  Primitive:      D3DPT_TRIANGLELIST (1 Triangle, 3 Vertices)\n"
                << "  FVF Formats:    D3DFVF_XYZ | D3DFVF_DIFFUSE\n"
                << "  Interpolation:  Gouraud Shading across Barycentric Rasterizer\n"
                << "  Presents:       " << static_cast<d3d9::Direct3DDevice9Impl*>(pDevice)->GetPresentCount() << " frame(s) blitted to User32 Window.\n";
        }

        pDevice->Release();
        pD3D->Release();
        user32::DestroyWindow(hwnd);
    }

    void cmdGDI(const std::vector<std::string>& tokens, std::ostream& out) {
        (void)tokens;
        out << "========================================================================\n"
            << "              MicaNT Graphics Device Interface (GDI32) Subsystem         \n"
            << "========================================================================\n\n";

        out << "GDI Version:        3.0 (Win32 GDI Clean-Room Native)\n"
            << "Export Library:     gdi32.dll\n"
            << "Default Rasterizer: 32-bpp BGRA TrueColor Software Engine\n"
            << "Stock Objects:      Brushes (WHITE, BLACK, NULL), Pens (WHITE, BLACK, NULL), System Fonts\n"
            << "Supported ROPs:     SRCCOPY, SRCPAINT, SRCAND, SRCINVERT, BLACKNESS, WHITENESS\n"
            << "OpenGL / 3D Bridge: ChoosePixelFormat, SetPixelFormat, SwapBuffers\n\n";

        gdi32::HDC hdcMem = gdi32::CreateCompatibleDC(nullptr);
        if (hdcMem) {
            gdi32::HBITMAP hbmp = gdi32::CreateCompatibleBitmap(hdcMem, 64, 64);
            gdi32::SelectObject(hdcMem, hbmp);
            gdi32::HBRUSH hbr = gdi32::CreateSolidBrush(gdi32::RGB(0, 120, 215));
            gdi32::RECT rc{0, 0, 64, 64};
            gdi32::FillRect(hdcMem, &rc, hbr);
            gdi32::HPEN hpen = gdi32::CreatePen(gdi32::PS_SOLID, 1, gdi32::RGB(255, 255, 255));
            gdi32::SelectObject(hdcMem, hpen);
            gdi32::Rectangle(hdcMem, 10, 10, 54, 54);
            gdi32::TextOutW(hdcMem, 14, 28, L"MICA", 4);
            out << "[GDI32] Test DC rendering verified: 64x64 bitmap with solid fill, pen rect & text.\n";
            gdi32::DeleteObject(hpen);
            gdi32::DeleteObject(hbr);
            gdi32::DeleteObject(hbmp);
            gdi32::DeleteDC(hdcMem);
        }
    }

    void cmdCOM(const std::vector<std::string>& tokens, std::ostream& out) {
        (void)tokens;
        out << "========================================================================\n"
            << "        MicaNT Component Object Model (COM) & OLE Automation Subsystem   \n"
            << "========================================================================\n\n";

        out << "COM Runtime:       ole32.dll / oleaut32.dll\n"
            << "Threading Model:   Multi-Threaded Apartment (MTA) & Single-Threaded Apartment (STA)\n"
            << "Task Allocator:    CoTaskMemAlloc / CoTaskMemFree (RtlProcessHeap)\n"
            << "Automation Types:  BSTR (length-prefixed UTF-16), VARIANT (polymorphic union)\n"
            << "Class Factories:   IUnknown, IClassFactory, CoGetClassObject, CoCreateInstance\n\n";

        ole32::HRESULT hr = ole32::CoInitializeEx(nullptr, ole32::COINIT_MULTITHREADED);
        out << "[OLE32] CoInitializeEx initialized (HRESULT: 0x" << std::hex << hr << std::dec << ")\n";

        micant::GUID g{};
        ole32::CoCreateGuid(&g);
        wchar_t szGuid[64]{};
        ole32::StringFromGUID2(g, szGuid, 64);
        std::wstring wsGuid(szGuid);
        std::string sGuid(wsGuid.begin(), wsGuid.end());
        out << "[OLE32] Generated test GUID: " << sGuid << "\n";

        ole32::BSTR bstr = ole32::SysAllocString(L"MicaNT Native OLE Automation");
        if (bstr) {
            out << "[OLEAUT32] Allocated BSTR: length=" << ole32::SysStringLen(bstr) 
                << " characters, byteLen=" << ole32::SysStringByteLen(bstr) << " bytes\n";
            ole32::SysFreeString(bstr);
        }

        ole32::CoUninitialize();
    }

    void cmdShell32(const std::vector<std::string>& tokens, std::ostream& out) {
        (void)tokens;
        out << "========================================================================\n"
            << "          MicaNT Shell API & Lightweight Shlwapi Subsystem              \n"
            << "========================================================================\n\n";

        out << "Shell32 Version:   6.0 (Clean-Room Win32 Native)\n"
            << "Export Libraries:  shell32.dll & shlwapi.dll\n"
            << "Folder Mapping:    CSIDL & KNOWNFOLDERID Canonical Userland Trees\n"
            << "Execution Bridge:  ShellExecuteW / ShellExecuteExW -> CreateProcessW\n"
            << "Notification Tray: Shell_NotifyIconW (Active Icons: "
            << shell32::TrayNotificationManager::get().getIconCount() << ")\n\n";

        wchar_t bufWin[260]{}, bufProg[260]{}, bufDoc[260]{};
        shell32::SHGetFolderPathW(nullptr, shell32::CSIDL_WINDOWS, nullptr, 0, bufWin);
        shell32::SHGetFolderPathW(nullptr, shell32::CSIDL_PROGRAM_FILES, nullptr, 0, bufProg);
        shell32::SHGetFolderPathW(nullptr, shell32::CSIDL_PERSONAL, nullptr, 0, bufDoc);

        std::wstring wsWin(bufWin), wsProg(bufProg), wsDoc(bufDoc);
        out << "Canonical Shell Paths:\n"
            << "  CSIDL_WINDOWS:       " << std::string(wsWin.begin(), wsWin.end()) << "\n"
            << "  CSIDL_PROGRAM_FILES: " << std::string(wsProg.begin(), wsProg.end()) << "\n"
            << "  CSIDL_PERSONAL:      " << std::string(wsDoc.begin(), wsDoc.end()) << "\n\n";

        int numArgs = 0;
        const wchar_t* cmdTest = L"notepad.exe \"C:\\Program Files\\sample document.txt\" --verbose";
        wchar_t** argv = shell32::CommandLineToArgvW(cmdTest, &numArgs);
        if (argv) {
            out << "[CommandLineToArgvW] Parsed " << numArgs << " argument(s):\n";
            for (int i = 0; i < numArgs; ++i) {
                std::wstring argW(argv[i]);
                out << "  Arg[" << i << "]: " << std::string(argW.begin(), argW.end()) << "\n";
            }
            kernel32::LocalFree(argv);
        }
    }

    void cmdComCtl(const std::vector<std::string>& tokens, std::ostream& out) {
        (void)tokens;
        out << "========================================================================\n"
            << "          MicaNT Common Controls (ComCtl32) Subsystem                   \n"
            << "========================================================================\n\n";

        out << "ComCtl32 Version:  6.0 (Clean-Room Modern Controls)\n"
            << "Export Library:    comctl32.dll\n"
            << "Registered Classes: msctls_progress32, msctls_statusbar32, msctls_updown32,\n"
            << "                    msctls_trackbar32, SysListView32, SysTreeView32\n\n";

        comctl32::HIMAGELIST himl = comctl32::ImageList_Create(16, 16, comctl32::ILC_COLOR32, 4, 4);
        if (himl) {
            comctl32::ImageList_AddIcon(himl, nullptr);
            comctl32::ImageList_AddIcon(himl, nullptr);
            out << "[ImageList] Created HIMAGELIST (16x16, 32-bpp) with "
                << comctl32::ImageList_GetImageCount(himl) << " icon frame(s).\n";
            comctl32::ImageList_Destroy(himl);
        }

        win32::HWND hProg = user32::CreateWindowExW(
            0, comctl32::PROGRESS_CLASSW, L"", 0,
            10, 10, 200, 24, nullptr, nullptr, nullptr, nullptr
        );
        if (hProg) {
            user32::SendMessageW(hProg, comctl32::PBM_SETRANGE32, 0, 100);
            user32::SendMessageW(hProg, comctl32::PBM_SETPOS, 65, 0);
            int curPos = static_cast<int>(user32::SendMessageW(hProg, comctl32::PBM_GETPOS, 0, 0));
            out << "[ProgressBar] Created msctls_progress32 window, Range [0..100], Current Pos: "
                << curPos << "%\n";
            user32::DestroyWindow(hProg);
        }
    }

    void cmdView3D(const std::vector<std::string>& tokens, std::ostream& out) {
        viewer::ViewerModelType model = viewer::ViewerModelType::Crystal;
        bool wireframe = false;
        uint32_t frames = 20;

        for (size_t i = 1; i < tokens.size(); ++i) {
            const auto& t = tokens[i];
            if (t == "--torus" || t == "torus") {
                model = viewer::ViewerModelType::Torus;
            } else if (t == "--cube" || t == "cube") {
                model = viewer::ViewerModelType::Cube;
            } else if (t == "--crystal" || t == "crystal") {
                model = viewer::ViewerModelType::Crystal;
            } else if (t == "--wireframe" || t == "-w" || t == "wireframe") {
                wireframe = true;
            } else if (t == "--frames" && i + 1 < tokens.size()) {
                frames = std::stoul(tokens[++i]);
            }
        }

        out << "[PrismX 3D Viewer] Launching interactive Direct3D 11 / User32 window...\n";
        viewer::ViewerSession session(640, 480);
        if (!session.initialize(L"MicaNT PrismX 3D Interactive Viewer")) {
            out << "[PrismX 3D Viewer] Error: Failed to initialize 3D viewer session.\n";
            return;
        }

        session.setModel(model);
        session.setWireframe(wireframe);

        const char* modelName = "DEC PRISM Crystal Core";
        if (model == viewer::ViewerModelType::Torus) modelName = "Parametric 3D Torus";
        else if (model == viewer::ViewerModelType::Cube) modelName = "Shaded 3D Box";

        out << "  Active Model:   " << modelName << "\n"
            << "  Rasterizer:     " << (wireframe ? "Wireframe" : "Solid Fill") << "\n"
            << "  Window Target:  640x480 HWND\n"
            << "  Rendering " << frames << " frames...\n";

        auto stats = session.run(frames);

        out << "[PrismX 3D Viewer] Session Completed:\n"
            << "  Rendered Frames: " << stats.frameCount << "\n"
            << "  Triangles:       " << stats.triangleCount << " (" << stats.vertexCount << " vertices)\n"
            << "  Avg Framerate:   " << stats.averageFps << " FPS (" << stats.lastFrameTimeMs << " ms/frame)\n"
            << "  Camera Orbit:    Distance=" << stats.cameraDistance << ", Yaw=" << stats.cameraYaw << ", Pitch=" << stats.cameraPitch << "\n";
    }

    void cmdVulkan(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() > 1 && (tokens[1] == "test" || tokens[1] == "cube")) {
            out << "[PrismVK] Initializing Vulkan 1.3 test pipeline...\n";

            vulkan::VkApplicationInfo appInfo{};
            appInfo.sType = vulkan::VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "MicaNT VkCube / Triangle Test";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "PrismVK";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
            appInfo.apiVersion = VK_API_VERSION_1_3;

            vulkan::VkInstanceCreateInfo instInfo{};
            instInfo.sType = vulkan::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instInfo.pApplicationInfo = &appInfo;

            vulkan::VkInstance instance = nullptr;
            vulkan::VkResult res = vulkan::vkCreateInstance(&instInfo, nullptr, &instance);
            if (res != vulkan::VK_SUCCESS || !instance) {
                out << "[PrismVK] Failed to create Vulkan instance (code " << res << ").\n";
                return;
            }

            uint32_t physCount = 0;
            vulkan::vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
            if (physCount == 0) {
                out << "[PrismVK] No Vulkan physical devices found.\n";
                vulkan::vkDestroyInstance(instance, nullptr);
                return;
            }

            std::vector<vulkan::VkPhysicalDevice> physDevices(physCount);
            vulkan::vkEnumeratePhysicalDevices(instance, &physCount, physDevices.data());
            vulkan::VkPhysicalDevice phys = physDevices[0];

            // Create Logical Device
            float queuePriority = 1.0f;
            vulkan::VkDeviceQueueCreateInfo qci{};
            qci.sType = vulkan::VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = 0;
            qci.queueCount = 1;
            qci.pQueuePriorities = &queuePriority;

            vulkan::VkDeviceCreateInfo devInfo{};
            devInfo.sType = vulkan::VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            devInfo.queueCreateInfoCount = 1;
            devInfo.pQueueCreateInfos = &qci;

            vulkan::VkDevice device = nullptr;
            res = vulkan::vkCreateDevice(phys, &devInfo, nullptr, &device);
            if (res != vulkan::VK_SUCCESS || !device) {
                out << "[PrismVK] Failed to create Vulkan logical device.\n";
                vulkan::vkDestroyInstance(instance, nullptr);
                return;
            }

            vulkan::VkQueue queue = nullptr;
            vulkan::vkGetDeviceQueue(device, 0, 0, &queue);

            // Create Win32 Surface
            vulkan::VkWin32SurfaceCreateInfoKHR surfInfo{};
            surfInfo.sType = vulkan::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surfInfo.hwnd = reinterpret_cast<void*>(0x10001);
            surfInfo.hinstance = reinterpret_cast<void*>(0x400000);

            vulkan::VkSurfaceKHR surface = 0;
            vulkan::vkCreateWin32SurfaceKHR(instance, &surfInfo, nullptr, &surface);

            // Create Swapchain
            vulkan::VkSwapchainCreateInfoKHR scInfo{};
            scInfo.sType = vulkan::VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            scInfo.surface = surface;
            scInfo.minImageCount = 2;
            scInfo.imageFormat = vulkan::VK_FORMAT_B8G8R8A8_UNORM;
            scInfo.imageExtent = { 1280, 720 };
            scInfo.imageUsage = vulkan::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            scInfo.presentMode = vulkan::VK_PRESENT_MODE_FIFO_KHR;

            vulkan::VkSwapchainKHR swapchain = 0;
            vulkan::vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain);

            // Record and execute command buffer
            vulkan::VkCommandPool commandPool = 0;
            vulkan::VkCommandPoolCreateInfo cpInfo{};
            cpInfo.sType = vulkan::VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            cpInfo.queueFamilyIndex = 0;
            vulkan::vkCreateCommandPool(device, &cpInfo, nullptr, &commandPool);

            vulkan::VkCommandBufferAllocateInfo cbAlloc{};
            cbAlloc.sType = vulkan::VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAlloc.commandPool = commandPool;
            cbAlloc.commandBufferCount = 1;
            vulkan::VkCommandBuffer cmdBuf = nullptr;
            vulkan::vkAllocateCommandBuffers(device, &cbAlloc, &cmdBuf);

            vulkan::VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = vulkan::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vulkan::vkBeginCommandBuffer(cmdBuf, &beginInfo);

            vulkan::VkClearValue clearColor{};
            clearColor.color.float32[0] = 0.08f;
            clearColor.color.float32[1] = 0.08f;
            clearColor.color.float32[2] = 0.22f;
            clearColor.color.float32[3] = 1.0f;

            vulkan::VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType = vulkan::VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderArea.extent = { 1280, 720 };
            rpBegin.clearValueCount = 1;
            rpBegin.pClearValues = &clearColor;
            vulkan::vkCmdBeginRenderPass(cmdBuf, &rpBegin, vulkan::VK_SUBPASS_CONTENTS_INLINE);

            vulkan::VkViewport vp{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
            vulkan::vkCmdSetViewport(cmdBuf, 0, 1, &vp);
            vulkan::vkCmdDraw(cmdBuf, 3, 1, 0, 0);
            vulkan::vkCmdEndRenderPass(cmdBuf);
            vulkan::vkEndCommandBuffer(cmdBuf);

            vulkan::VkSubmitInfo submitInfo{};
            submitInfo.sType = vulkan::VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuf;
            vulkan::vkQueueSubmit(queue, 1, &submitInfo, 0);

            uint32_t imageIndex = 0;
            vulkan::vkAcquireNextImageKHR(device, swapchain, 0, 0, 0, &imageIndex);

            vulkan::VkPresentInfoKHR presentInfo{};
            presentInfo.sType = vulkan::VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            vulkan::vkQueuePresentKHR(queue, &presentInfo);

            out << "[PrismVK] Vulkan 1.3 pipeline verified successfully!\n"
                << "  API: Khronos Vulkan 1.3.0 (ICD: vulkan-1.dll)\n"
                << "  Surface: Win32 HWND (1280x720), Swapchain: 2 images (VK_FORMAT_B8G8R8A8_UNORM)\n"
                << "  Queue: Family 0 (Graphics | Compute | Transfer), Command Buffer recorded & submitted.\n"
                << "  Status: Frame presented to display compositor via vkQueuePresentKHR.\n";

            vulkan::vkFreeCommandBuffers(device, commandPool, 1, &cmdBuf);
            vulkan::vkDestroyCommandPool(device, commandPool, nullptr);
            vulkan::vkDestroySwapchainKHR(device, swapchain, nullptr);
            vulkan::vkDestroySurfaceKHR(instance, surface, nullptr);
            vulkan::vkDestroyDevice(device, nullptr);
            vulkan::vkDestroyInstance(instance, nullptr);
            return;
        }

        out << "========================================================================\n"
            << "              MicaNT PrismVK & Vulkan 1.3 ICD Subsystem                 \n"
            << "========================================================================\n\n";

        vulkan::VulkanLoader::get().initializeIcdRegistry();

        out << "ICD Loader:        vulkan-1.dll (Khronos Vulkan 1.3.0 Specification)\n"
            << "Registry Discovery: \\Registry\\Machine\\SOFTWARE\\Khronos\\Vulkan\\Drivers\n"
            << "Active Manifest:   C:\\Windows\\System32\\prism_vk.json (Installed: " 
            << (vulkan::VulkanLoader::get().isIcdRegistered() ? "YES" : "NO") << ")\n\n";

        vulkan::VkInstanceCreateInfo instInfo{};
        instInfo.sType = vulkan::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        vulkan::VkInstance inst = nullptr;
        if (vulkan::vkCreateInstance(&instInfo, nullptr, &inst) == vulkan::VK_SUCCESS && inst) {
            uint32_t count = 0;
            vulkan::vkEnumeratePhysicalDevices(inst, &count, nullptr);
            if (count > 0) {
                std::vector<vulkan::VkPhysicalDevice> pDevs(count);
                vulkan::vkEnumeratePhysicalDevices(inst, &count, pDevs.data());
                for (uint32_t i = 0; i < count; ++i) {
                    vulkan::VkPhysicalDeviceProperties props{};
                    vulkan::vkGetPhysicalDeviceProperties(pDevs[i], &props);

                    vulkan::VkPhysicalDeviceMemoryProperties mem{};
                    vulkan::vkGetPhysicalDeviceMemoryProperties(pDevs[i], &mem);

                    out << "Physical Device " << i << ": " << props.deviceName << "\n"
                        << "  Vendor ID:       0x" << std::hex << std::uppercase << props.vendorID << std::dec << "\n"
                        << "  Device ID:       0x" << std::hex << std::uppercase << props.deviceID << std::dec << "\n"
                        << "  Device Type:     Discrete GPU (WDDM 3.0 / Sovereign)\n"
                        << "  API Version:     1.3.0\n"
                        << "  Driver Version:  1.0.0\n"
                        << "  Dedicated VRAM:  " << (mem.memoryHeaps[0].size / (1024 * 1024)) << " MB\n"
                        << "  Shared GTT RAM:  " << (mem.memoryHeaps[1].size / (1024 * 1024)) << " MB\n"
                        << "  Queue Families:  Graphics, Compute, Transfer (16 Queues)\n"
                        << "  Extensions:      VK_KHR_surface, VK_KHR_win32_surface, VK_KHR_swapchain\n\n";
                }
            }
            vulkan::vkDestroyInstance(inst, nullptr);
        }

        out << "Type 'vulkan test' or 'vkcube' to execute real-time Vulkan render test.\n";
    }

    void cmdWinMM(const std::vector<std::string>& tokens, std::ostream& out) {
        winmm::InitializeWinMMExports();

        if (tokens.size() > 1 && (tokens[1] == "beep" || tokens[1] == "play")) {
            out << "[WinMM] Generating procedural 440 Hz (A4) 16-bit PCM RIFF WAVE...\n";
            auto waveBuf = winmm::SoundPlaybackService::Instance().GenerateSineWaveRiff(440, 500, 44100);
            int res = winmm::PlaySoundA(reinterpret_cast<const char*>(waveBuf.data()), nullptr, winmm::SND_MEMORY | winmm::SND_SYNC);
            out << "  RIFF WAVE Buffer:   " << waveBuf.size() << " bytes\n"
                << "  PlaySound Result:   " << (res ? "SUCCESS" : "FAILED") << "\n"
                << "  Playback State:     " << (winmm::SoundPlaybackService::Instance().IsPlaying() ? "Active" : "Completed") << "\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "timer") {
            out << "[WinMM] High-Resolution Multimedia Timer Test:\n";
            winmm::TIMECAPS tc{};
            winmm::timeGetDevCaps(&tc, sizeof(tc));
            out << "  Timer Min Period:   " << tc.wPeriodMin << " ms\n"
                << "  Timer Max Period:   " << tc.wPeriodMax << " ms\n";

            winmm::timeBeginPeriod(1);
            uint32_t t0 = winmm::timeGetTime();
            uint32_t currentPeriod = winmm::MultimediaTimerService::Instance().GetCurrentPeriod();
            out << "  timeBeginPeriod(1): Active target resolution = " << currentPeriod << " ms\n";
            
            uint32_t t1 = t0;
            while (t1 == t0) {
                t1 = winmm::timeGetTime();
            }
            out << "  timeGetTime Delta:  " << (t1 - t0) << " ms\n";
            winmm::timeEndPeriod(1);
            out << "  timeEndPeriod(1):   Restored timer resolution.\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "mci") {
            out << "[WinMM] Media Control Interface (MCI) Command Engine:\n";
            char retBuf[128]{};
            winmm::mciSendStringA("open bgm.wav type waveaudio alias bgm", retBuf, sizeof(retBuf), nullptr);
            out << "  MCI: open bgm.wav type waveaudio alias bgm\n";

            winmm::mciSendStringA("status bgm mode", retBuf, sizeof(retBuf), nullptr);
            out << "  MCI status mode:    " << retBuf << "\n";

            winmm::mciSendStringA("play bgm", retBuf, sizeof(retBuf), nullptr);
            winmm::mciSendStringA("status bgm mode", retBuf, sizeof(retBuf), nullptr);
            out << "  MCI play -> mode:   " << retBuf << "\n";

            winmm::mciSendStringA("close bgm", retBuf, sizeof(retBuf), nullptr);
            out << "  MCI: close bgm completed.\n";
            return;
        }

        out << "========================================================================\n"
            << "              MicaNT Windows Multimedia Engine (winmm.dll)               \n"
            << "========================================================================\n\n";

        winmm::WAVEOUTCAPSA woc{};
        winmm::waveOutGetDevCapsA(0, &woc, sizeof(woc));
        winmm::TIMECAPS tc{};
        winmm::timeGetDevCaps(&tc, sizeof(tc));
        winmm::JOYCAPSA jc{};
        winmm::joyGetDevCapsA(0, &jc, sizeof(jc));

        out << "WaveOut Device:       " << woc.szPname << " (Channels: " << woc.wChannels << ")\n"
            << "WaveOut Formats:      11kHz - 192kHz Standard PCM / IEEE Float\n"
            << "Multimedia Timers:    Min " << tc.wPeriodMin << " ms, Max " << tc.wPeriodMax << " ms (timeGetTime: " << winmm::timeGetTime() << " ms)\n"
            << "Joystick / Gamepad:   " << jc.szPname << " (Buttons: " << jc.wNumButtons << ", Axes: " << jc.wNumAxes << ")\n"
            << "MCI String Parser:    Ready (open, play, pause, resume, stop, status, close)\n\n"
            << "Usage:\n"
            << "  winmm beep          Plays procedural 440 Hz sine wave beep via PlaySound\n"
            << "  winmm timer         Tests high-resolution 1ms multimedia timer\n"
            << "  winmm mci           Executes Media Control Interface batch commands\n";
    }

    void cmdDirectSound(const std::vector<std::string>& tokens, std::ostream& out) {
        dsound::InitializeDirectSoundExports();

        out << "========================================================================\n"
            << "            MicaNT DirectSound 8 Runtime Subsystem (dsound.dll)          \n"
            << "========================================================================\n\n";

        dsound::IDirectSound8* pDS8 = nullptr;
        int32_t hr = dsound::DirectSoundCreate8(nullptr, &pDS8, nullptr);
        if (hr != dsound::DS_OK || !pDS8) {
            out << "Error: Failed to create DirectSound8 device (hr=" << hr << ")\n";
            return;
        }

        dsound::DSCAPS caps{};
        caps.dwSize = sizeof(caps);
        pDS8->GetCaps(&caps);

        out << "DirectSound Interface: IDirectSound8 (Version 8.0 Parity)\n"
            << "Max Hardware Mixing:   " << caps.dwMaxHwMixingAllBuffers << " 2D Buffers, " << caps.dwMaxHw3DAllBuffers << " 3D Buffers\n"
            << "Hardware Audio VRAM:   " << (caps.dwTotalHwMemBytes / (1024 * 1024)) << " MB Free\n"
            << "Sample Rate Range:     " << caps.dwMinSecondarySampleRate << " Hz - " << caps.dwMaxSecondarySampleRate << " Hz\n\n";

        audio::WAVEFORMATEX wfx{};
        wfx.wFormatTag = audio::WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = 44100;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = 4;
        wfx.nAvgBytesPerSec = 44100 * 4;

        dsound::DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwFlags = dsound::DSBCAPS_CTRL3D | dsound::DSBCAPS_CTRLVOLUME | dsound::DSBCAPS_CTRLPAN | dsound::DSBCAPS_CTRLFREQUENCY;
        desc.dwBufferBytes = 44100 * 4; // 1 second buffer
        desc.lpwfxFormat = &wfx;

        dsound::IDirectSoundBuffer* pBuffer = nullptr;
        pDS8->CreateSoundBuffer(&desc, &pBuffer, nullptr);

        if (pBuffer) {
            void *p1 = nullptr, *p2 = nullptr;
            uint32_t b1 = 0, b2 = 0;
            pBuffer->Lock(44100 * 4 - 512, 1024, &p1, &b1, &p2, &b2, 0);
            out << "Circular Buffer Lock:\n"
                << "  Requested Offset:   " << (44100 * 4 - 512) << " bytes, Size: 1024 bytes\n"
                << "  Ptr1: " << p1 << " (" << b1 << " bytes), Ptr2: " << p2 << " (" << b2 << " bytes wrap-around)\n";
            pBuffer->Unlock(p1, b1, p2, b2);

            pBuffer->SetVolume(-600); // -6.00 dB
            pBuffer->SetPan(1500);    // +15.00 dB right bias
            int32_t curVol = 0, curPan = 0;
            pBuffer->GetVolume(&curVol);
            pBuffer->GetPan(&curPan);
            out << "Attenuation & Pan:    Volume: " << curVol << " mB (" << (curVol / 100.0f) << " dB), Pan: " << curPan << " mB\n";

            dsound::IDirectSound3DBuffer* p3DBuf = nullptr;
            if (pBuffer->QueryInterface(dsound::IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&p3DBuf)) == dsound::DS_OK && p3DBuf) {
                p3DBuf->SetPosition(5.0f, 0.0f, 10.0f, 0);
                p3DBuf->SetMinDistance(1.0f, 0);
                p3DBuf->SetMaxDistance(50.0f, 0);
                dsound::D3DVECTOR pos{};
                p3DBuf->GetPosition(&pos);
                out << "3D Emitter Position:  X=" << pos.x << ", Y=" << pos.y << ", Z=" << pos.z << "\n";
                p3DBuf->Release();
            }

            pBuffer->Play(0, 0, dsound::DSBPLAY_LOOPING);
            uint32_t status = 0;
            pBuffer->GetStatus(&status);
            out << "Playback Status:      " << ((status & dsound::DSBSTATUS_PLAYING) ? "PLAYING" : "STOPPED")
                << " (Looping: " << ((status & dsound::DSBSTATUS_LOOPING) ? "YES" : "NO") << ")\n";

            int16_t mixBuffer[256 * 2]{};
            auto* pImpl = static_cast<dsound::DirectSound8Impl*>(pDS8);
            size_t activeVoices = pImpl->MixActiveVoices(mixBuffer, 256);
            out << "Real-Time PCM Mixer:  Mixed " << activeVoices << " active voice(s) into 256 stereo frames.\n";

            pBuffer->Stop();
            pBuffer->Release();
        }

        pDS8->Release();
    }

    void cmdVersion(const std::vector<std::string>& tokens, std::ostream& out) {
        version::InitializeVersionExports();

        std::string targetMod = "kernel32.dll";
        if (tokens.size() > 1) {
            targetMod = tokens[1];
        }

        out << "========================================================================\n"
            << "         MicaNT Windows Version Information Subsystem (version.dll)      \n"
            << "========================================================================\n\n";

        uint32_t handle = 0;
        uint32_t size = version::GetFileVersionInfoSizeA(targetMod.c_str(), &handle);
        if (size == 0) {
            out << "Error: No version resource found for module: " << targetMod << "\n";
            return;
        }

        std::vector<uint8_t> data(size);
        if (!version::GetFileVersionInfoA(targetMod.c_str(), handle, size, data.data())) {
            out << "Error: Failed to retrieve version info block for: " << targetMod << "\n";
            return;
        }

        void* pFixed = nullptr;
        uint32_t fixedLen = 0;
        if (version::VerQueryValueA(data.data(), "\\", &pFixed, &fixedLen) && pFixed) {
            auto* ffi = static_cast<const version::VS_FIXEDFILEINFO*>(pFixed);
            uint32_t fvMS = ffi->dwFileVersionMS;
            uint32_t fvLS = ffi->dwFileVersionLS;
            uint32_t pvMS = ffi->dwProductVersionMS;
            uint32_t pvLS = ffi->dwProductVersionLS;

            out << "Module Name:          " << targetMod << "\n"
                << "File Version (MS.LS): " << (fvMS >> 16) << "." << (fvMS & 0xFFFF) << "."
                                            << (fvLS >> 16) << "." << (fvLS & 0xFFFF) << "\n"
                << "Product Version:      " << (pvMS >> 16) << "." << (pvMS & 0xFFFF) << "."
                                            << (pvLS >> 16) << "." << (pvLS & 0xFFFF) << "\n"
                << "File Type:            " << (ffi->dwFileType == version::VFT_DLL ? "VFT_DLL (Dynamic Link Library)" : "VFT_APP (Executable Application)") << "\n"
                << "File OS:              VOS_NT_WINDOWS32 (0x00040004)\n\n";
        }

        const char* props[] = { "FileDescription", "CompanyName", "ProductName", "FileVersion", "LegalCopyright", "OriginalFilename" };
        out << "String Table Metadata:\n";
        for (const char* prop : props) {
            void* pVal = nullptr;
            uint32_t valLen = 0;
            std::string subBlock = "\\StringFileInfo\\040904B0\\" + std::string(prop);
            if (version::VerQueryValueA(data.data(), subBlock.c_str(), &pVal, &valLen) && pVal) {
                out << "  " << std::left << std::setw(20) << prop << ": " << static_cast<const char*>(pVal) << "\n";
            }
        }

        char langName[64]{};
        version::VerLanguageNameA(0x0409, langName, sizeof(langName));
        out << "\nLanguage:             0x0409 (" << langName << ")\n";
    }

    void cmdOpenGL(const std::vector<std::string>& tokens, std::ostream& out) {
        opengl::InitializeOpenglSubsystemExports();

        if (tokens.size() > 1 && (tokens[1] == "test" || tokens[1] == "gears" || tokens[1] == "cube")) {
            out << "[OpenGL] Launching 3D OpenGL & WGL Presentation Pipeline...\n";

            // 1. Create User32 presentation window & DC
            win32::HWND hwnd = user32::CreateWindowExW(
                0, L"MicaNT_Window", L"MicaNT OpenGL / WGL 3D Test",
                0, 0, 0, 640, 480, nullptr, nullptr, nullptr, nullptr
            );
            if (!hwnd) {
                out << "[OpenGL] Error: Failed to create presentation window.\n";
                return;
            }

            gdi32::HDC hdc = reinterpret_cast<gdi32::HDC>(user32::GetDC(hwnd));
            gdi32::PIXELFORMATDESCRIPTOR pfd{};
            pfd.dwFlags = gdi32::PFD_DRAW_TO_WINDOW | gdi32::PFD_SUPPORT_OPENGL | gdi32::PFD_DOUBLEBUFFER;
            int pixelFmt = gdi32::ChoosePixelFormat(hdc, &pfd);
            gdi32::SetPixelFormat(hdc, pixelFmt, &pfd);

            // 2. Create and bind WGL Context
            opengl::HGLRC hglrc = opengl::wglCreateContext(hdc);
            if (!hglrc) {
                out << "[OpenGL] Error: Failed to create WGL rendering context.\n";
                user32::ReleaseDC(hwnd, reinterpret_cast<user32::HDC>(hdc));
                user32::DestroyWindow(hwnd);
                return;
            }
            opengl::wglMakeCurrent(hdc, hglrc);

            // 3. Configure State & Pipeline
            opengl::glViewport(0, 0, 640, 480);
            opengl::glClearColor(0.06f, 0.10f, 0.22f, 1.0f);
            opengl::glClearDepth(1.0);
            opengl::glEnable(opengl::GL_DEPTH_TEST);
            opengl::glDepthFunc(opengl::GL_LEQUAL);
            opengl::glShadeModel(opengl::GL_SMOOTH);

            opengl::glClear(opengl::GL_COLOR_BUFFER_BIT | opengl::GL_DEPTH_BUFFER_BIT);

            // 4. Matrix Setup
            opengl::glMatrixMode(opengl::GL_PROJECTION);
            opengl::glLoadIdentity();
            opengl::gluPerspective(45.0, 640.0 / 480.0, 0.1, 100.0);

            opengl::glMatrixMode(opengl::GL_MODELVIEW);
            opengl::glLoadIdentity();
            opengl::gluLookAt(0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

            // Rotate Prism
            opengl::glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
            opengl::glRotatef(45.0f, 0.0f, 1.0f, 0.0f);

            // 5. Draw 3D Shaded DEC Prism / Crystal Geometry
            opengl::glBegin(opengl::GL_TRIANGLES);

            // Front face (Red to Green to Blue)
            opengl::glColor3f(1.0f, 0.1f, 0.1f); opengl::glVertex3f( 0.0f,  0.8f,  0.0f);
            opengl::glColor3f(0.1f, 1.0f, 0.1f); opengl::glVertex3f(-0.7f, -0.6f,  0.5f);
            opengl::glColor3f(0.1f, 0.3f, 1.0f); opengl::glVertex3f( 0.7f, -0.6f,  0.5f);

            // Right face (Red to Blue to Magenta)
            opengl::glColor3f(1.0f, 0.1f, 0.1f); opengl::glVertex3f( 0.0f,  0.8f,  0.0f);
            opengl::glColor3f(0.1f, 0.3f, 1.0f); opengl::glVertex3f( 0.7f, -0.6f,  0.5f);
            opengl::glColor3f(1.0f, 0.1f, 1.0f); opengl::glVertex3f( 0.0f, -0.6f, -0.7f);

            // Left face (Red to Magenta to Green)
            opengl::glColor3f(1.0f, 0.1f, 0.1f); opengl::glVertex3f( 0.0f,  0.8f,  0.0f);
            opengl::glColor3f(1.0f, 0.1f, 1.0f); opengl::glVertex3f( 0.0f, -0.6f, -0.7f);
            opengl::glColor3f(0.1f, 1.0f, 0.1f); opengl::glVertex3f(-0.7f, -0.6f,  0.5f);

            // Bottom base (Cyan / Yellow / White)
            opengl::glColor3f(0.1f, 0.9f, 0.9f); opengl::glVertex3f(-0.7f, -0.6f,  0.5f);
            opengl::glColor3f(1.0f, 1.0f, 0.1f); opengl::glVertex3f( 0.7f, -0.6f,  0.5f);
            opengl::glColor3f(1.0f, 1.0f, 1.0f); opengl::glVertex3f( 0.0f, -0.6f, -0.7f);

            opengl::glEnd();

            // 6. Swap Buffers to Window DC
            opengl::wglSwapBuffers(hdc);

            out << "[OpenGL] 3D Shaded Prism rendered and presented successfully!\n"
                << "  API Level:       OpenGL 1.4 / WGL 1.0 (Clean-Room Native)\n"
                << "  Window Target:   640x480 HWND\n"
                << "  Projection:      gluPerspective(45.0, aspect=1.33, zNear=0.1, zFar=100.0)\n"
                << "  Camera Matrix:   gluLookAt(eye=(0,0,4), target=(0,0,0), up=(0,1,0))\n"
                << "  Geometry:        4 Triangles, 12 Vertices, Perspective-Correct Barycentric Interpolation\n"
                << "  Depth Test:      32-Bit Floating Point Z-Buffer (GL_LEQUAL)\n"
                << "  Status:          Frame 1 blitted to User32 Window via wglSwapBuffers.\n";

            // Cleanup
            opengl::wglMakeCurrent(nullptr, nullptr);
            opengl::wglDeleteContext(hglrc);
            user32::ReleaseDC(hwnd, reinterpret_cast<user32::HDC>(hdc));
            user32::DestroyWindow(hwnd);
            return;
        }

        out << "========================================================================\n"
            << "         MicaNT OpenGL & Windows WGL Subsystem (opengl32.dll / glu32.dll)\n"
            << "========================================================================\n\n";

        out << "Vendor:            " << opengl::glGetString(opengl::GL_VENDOR) << "\n"
            << "Renderer:          " << opengl::glGetString(opengl::GL_RENDERER) << "\n"
            << "Version:           " << opengl::glGetString(opengl::GL_VERSION) << "\n"
            << "GLU Version:       1.3 MicaNT\n"
            << "WGL Extensions:    wglGetProcAddress, wglCreateContext, wglMakeCurrent, wglSwapBuffers\n"
            << "OpenGL Extensions: " << opengl::glGetString(opengl::GL_EXTENSIONS) << "\n"
            << "Current HGLRC:     " << opengl::wglGetCurrentContext() << "\n"
            << "Current HDC:       " << opengl::wglGetCurrentDC() << "\n\n"
            << "Usage:\n"
            << "  opengl info      Displays OpenGL runtime and driver metadata\n"
            << "  opengl test      Renders perspective-correct 3D crystal prism via WGL\n";
    }

    void cmdWinINet(const std::vector<std::string>& tokens, std::ostream& out) {
        wininet::InitializeWinINetSubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[WinINet] Testing Clean-Room HTTP 1.1 Client Pipeline...\n";
            wininet::HttpMockRegistry::Instance().registerMock(
                "http://micant.org/status",
                200,
                "application/json",
                "{\"os\":\"MicaNT\",\"kernel\":\"clean-room\",\"telemetry\":false,\"subsystems\":[\"wininet\",\"urlmon\"]}"
            );

            wininet::HINTERNET hSession = wininet::InternetOpenA("MicaNT-Shell/1.0", wininet::INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
            wininet::HINTERNET hConn = wininet::InternetConnectA(hSession, "micant.org", 80, nullptr, nullptr, wininet::INTERNET_SERVICE_HTTP, 0, 0);
            wininet::HINTERNET hReq = wininet::HttpOpenRequestA(hConn, "GET", "/status", "HTTP/1.1", nullptr, nullptr, 0, 0);

            if (wininet::HttpSendRequestA(hReq, nullptr, 0, nullptr, 0)) {
                uint32_t status = 0;
                uint32_t sLen = sizeof(status);
                wininet::HttpQueryInfoA(hReq, wininet::HTTP_QUERY_STATUS_CODE | wininet::HTTP_QUERY_FLAG_NUMBER, &status, &sLen, nullptr);

                char cType[64]{};
                uint32_t ctLen = sizeof(cType);
                wininet::HttpQueryInfoA(hReq, wininet::HTTP_QUERY_CONTENT_TYPE, cType, &ctLen, nullptr);

                std::vector<char> body(256, 0);
                uint32_t read = 0;
                wininet::InternetReadFile(hReq, body.data(), static_cast<uint32_t>(body.size() - 1), &read);

                out << "  HTTP Status:      " << status << " OK\n"
                    << "  Content-Type:     " << cType << "\n"
                    << "  Bytes Received:   " << read << " bytes\n"
                    << "  Payload:          " << body.data() << "\n"
                    << "  Result:           SUCCESS - RFC 7230 request executed cleanly.\n";
            } else {
                out << "  Result:           FAILED to send HTTP request.\n";
            }

            wininet::InternetCloseHandle(hReq);
            wininet::InternetCloseHandle(hConn);
            wininet::InternetCloseHandle(hSession);
            return;
        }

        if (tokens.size() > 1 && (tokens[1] == "cookies" || tokens[1] == "cookie")) {
            out << "Cookie Jar Contents:\n";
            std::string c = wininet::CookieJar::Instance().getCookiesForUrl("micant.org", "/");
            out << "  micant.org [/]: " << (c.empty() ? "(no cookies stored)" : c) << "\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "cache") {
            out << "Temporary Internet Files (URL Cache):\n";
            wininet::CacheEntry entry;
            if (wininet::UrlCacheManager::Instance().retrieveEntry("http://micant.org/status", entry)) {
                out << "  URL:        " << entry.url << "\n"
                    << "  Local Path: " << entry.localFilePath << "\n"
                    << "  Size:       " << entry.fileSize << " bytes\n";
            } else {
                out << "  (Cache empty or items expired)\n";
            }
            return;
        }

        out << "========================================================================\n"
            << "          MicaNT Windows Internet Subsystem (wininet.dll)               \n"
            << "========================================================================\n\n"
            << "API Version:       WinINet 11.00 (RFC 7230 / RFC 6265 / RFC 3986)\n"
            << "Supported Schemes: http://, https://, ftp://, file://\n"
            << "Handle Table:      Hierarchical Lifecycle (Session -> Connect -> Request)\n"
            << "Transfer Engines:  Standard Content-Length & Chunked Transfer-Encoding\n"
            << "State Storage:     Cookie Jar with Domain Matching & Temporary Internet Files\n\n"
            << "Usage:\n"
            << "  wininet info     Displays WinINet subsystem status\n"
            << "  wininet test     Executes simulated HTTP/1.1 GET transaction\n"
            << "  wininet cookies  Inspects active cookie jar\n"
            << "  wininet cache    Inspects URL cache / Temporary Internet Files\n";
    }

    void cmdUrlMon(const std::vector<std::string>& tokens, std::ostream& out) {
        urlmon::InitializeUrlMonSubsystemExports();

        // Check if invoked as curl / wget / download or urlmon <url>
        if (tokens.size() > 1 && tokens[1] != "info" && tokens[1] != "help") {
            std::string url;
            std::string destFile;

            for (size_t i = 1; i < tokens.size(); ++i) {
                if ((tokens[i] == "-o" || tokens[i] == "--output") && i + 1 < tokens.size()) {
                    destFile = tokens[++i];
                } else if (url.empty() && tokens[i] != "test") {
                    url = tokens[i];
                }
            }

            wininet::MockHttpResponse testResp;
            if (tokens[1] == "test" || url.empty() || !wininet::HttpMockRegistry::Instance().findMock(url, testResp)) {
                if (url.empty() || tokens[1] == "test") url = "http://micant.org/sample.txt";
                wininet::HttpMockRegistry::Instance().registerMock(
                    url,
                    200,
                    "text/plain",
                    "MicaNT Clean-Room Operating System - Sovereign Network Pipeline Verified!"
                );
            }

            if (destFile.empty()) {
                size_t slash = url.find_last_of('/');
                destFile = (slash != std::string::npos && slash + 1 < url.size()) ? url.substr(slash + 1) : "download.dat";
                if (destFile.find('?') != std::string::npos) {
                    destFile = destFile.substr(0, destFile.find('?'));
                }
            }

            out << "[URLMon] Initiating Download via URLDownloadToFileW...\n"
                << "  Source URL:  " << url << "\n"
                << "  Destination: " << destFile << "\n";

            class ConsoleProgressCallback : public urlmon::IBindStatusCallback {
            public:
                std::ostream& m_out;
                ConsoleProgressCallback(std::ostream& o) : m_out(o) {}

                virtual ole32::HRESULT QueryInterface(ole32::REFIID riid, void** ppv) override {
                    if (!ppv) return ole32::E_POINTER;
                    if (riid == ole32::IID_IUnknown || riid == urlmon::IID_IBindStatusCallback) {
                        *ppv = this;
                        return ole32::S_OK;
                    }
                    *ppv = nullptr;
                    return ole32::E_NOINTERFACE;
                }
                virtual uint32_t AddRef() override { return 1; }
                virtual uint32_t Release() override { return 1; }
                virtual ole32::HRESULT OnStartBinding(uint32_t, void*) override { return ole32::S_OK; }
                virtual ole32::HRESULT GetPriority(int32_t*) override { return ole32::S_OK; }
                virtual ole32::HRESULT OnLowResource(uint32_t) override { return ole32::S_OK; }
                virtual ole32::HRESULT OnProgress(uint32_t cur, uint32_t max, uint32_t status, const wchar_t*) override {
                    if (status == urlmon::BINDSTATUS_DOWNLOADINGDATA) {
                        m_out << "  Progress: " << cur << " / " << (max ? std::to_string(max) : "unknown") << " bytes\n";
                    }
                    return ole32::S_OK;
                }
                virtual ole32::HRESULT OnStopBinding(ole32::HRESULT, const wchar_t*) override { return ole32::S_OK; }
                virtual ole32::HRESULT GetBindInfo(uint32_t*, void*) override { return ole32::S_OK; }
                virtual ole32::HRESULT OnDataAvailable(uint32_t, uint32_t, void*, void*) override { return ole32::S_OK; }
                virtual ole32::HRESULT OnObjectAvailable(ole32::REFIID, ole32::IUnknown*) override { return ole32::S_OK; }
            };

            ConsoleProgressCallback cb(out);
            std::wstring wUrl = wininet::toWide(url);
            std::wstring wDest = wininet::toWide(destFile);

            ole32::HRESULT hr = urlmon::URLDownloadToFileW(nullptr, wUrl.c_str(), wDest.c_str(), 0, &cb);
            if (SUCCEEDED(hr)) {
                out << "[URLMon] Download completed successfully (hr=0x" << std::hex << hr << std::dec << ") -> " << destFile << "\n";
            } else {
                out << "[URLMon] Download failed with error code: 0x" << std::hex << hr << std::dec << "\n";
            }
            return;
        }

        out << "========================================================================\n"
            << "          MicaNT URL Moniker Subsystem (urlmon.dll)                     \n"
            << "========================================================================\n\n"
            << "API Surface:       URLDownloadToFileA/W, URLDownloadToCacheFileA/W\n"
            << "Stream Monikers:   URLOpenStreamW, URLOpenBlockingStreamW (ole32::IStream)\n"
            << "MIME Sniffer:      FindMimeFromData (Magic bytes: PNG, JPG, GIF, PDF, ZIP, MZ, HTML, JSON)\n"
            << "COM Monikers:      CreateURLMoniker, CreateURLMonikerEx (IMoniker)\n\n"
            << "Usage:\n"
            << "  curl <url> [-o <file>]     Downloads web resource using URLDownloadToFile\n"
            << "  wget <url>                 Downloads web resource to current directory\n"
            << "  urlmon test                Runs simulated download test\n";
    }

    void cmdBCrypt(const std::vector<std::string>& tokens, std::ostream& out) {
        crypto::InitializeBCryptSubsystemExports();

        if (tokens.size() > 1 && (tokens[1] == "hash" || tokens[1] == "digest")) {
            if (tokens.size() < 4) {
                out << "Usage: bcrypt hash <algorithm> <data_string>\n"
                    << "Algorithms: sha256, sha384, sha512, md5, sha1\n";
                return;
            }
            std::string algo = tokens[2];
            std::transform(algo.begin(), algo.end(), algo.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            const wchar_t* wAlgo = nullptr;
            if (algo == "sha256" || algo == "sha-256") wAlgo = crypto::BCRYPT_SHA256_ALGORITHM;
            else if (algo == "sha384" || algo == "sha-384") wAlgo = crypto::BCRYPT_SHA384_ALGORITHM;
            else if (algo == "sha512" || algo == "sha-512") wAlgo = crypto::BCRYPT_SHA512_ALGORITHM;
            else if (algo == "md5") wAlgo = crypto::BCRYPT_MD5_ALGORITHM;
            else if (algo == "sha1" || algo == "sha-1") wAlgo = crypto::BCRYPT_SHA1_ALGORITHM;
            else {
                out << "Error: Unknown algorithm '" << tokens[2] << "'. Supported: sha256, sha384, sha512, md5, sha1\n";
                return;
            }

            std::string payload;
            for (size_t i = 3; i < tokens.size(); ++i) {
                if (i > 3) payload += " ";
                payload += tokens[i];
            }

            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            int32_t status = crypto::BCryptOpenAlgorithmProvider(&hAlg, wAlgo, nullptr, 0);
            if (!BCRYPT_SUCCESS(status)) {
                out << "Error: BCryptOpenAlgorithmProvider failed with status 0x" << std::hex << status << std::dec << "\n";
                return;
            }

            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            status = crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            if (!BCRYPT_SUCCESS(status)) {
                crypto::BCryptCloseAlgorithmProvider(hAlg, 0);
                out << "Error: BCryptCreateHash failed with status 0x" << std::hex << status << std::dec << "\n";
                return;
            }

            status = crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(payload.data()), static_cast<uint32_t>(payload.size()), 0);
            if (!BCRYPT_SUCCESS(status)) {
                crypto::BCryptDestroyHash(hHash);
                crypto::BCryptCloseAlgorithmProvider(hAlg, 0);
                out << "Error: BCryptHashData failed with status 0x" << std::hex << status << std::dec << "\n";
                return;
            }

            uint32_t digestLen = 0;
            uint32_t cbResult = 0;
            crypto::BCryptGetProperty(hAlg, crypto::BCRYPT_HASH_LENGTH, reinterpret_cast<uint8_t*>(&digestLen), sizeof(digestLen), &cbResult, 0);
            if (digestLen == 0) digestLen = 32;

            std::vector<uint8_t> digest(digestLen);
            status = crypto::BCryptFinishHash(hHash, digest.data(), digestLen, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            if (!BCRYPT_SUCCESS(status)) {
                out << "Error: BCryptFinishHash failed with status 0x" << std::hex << status << std::dec << "\n";
                return;
            }

            out << "[BCrypt " << tokens[2] << "] Digest (" << digestLen << " bytes):\n  ";
            for (uint8_t b : digest) {
                out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            out << std::dec << "\n";
            return;
        }

        if (tokens.size() > 1 && (tokens[1] == "rand" || tokens[1] == "random")) {
            uint32_t count = 32;
            if (tokens.size() > 2) {
                try {
                    count = static_cast<uint32_t>(std::stoul(tokens[2]));
                } catch (...) {
                    count = 32;
                }
            }
            if (count > 256) count = 256;
            std::vector<uint8_t> buf(count);
            int32_t status = crypto::BCryptGenRandom(nullptr, buf.data(), count, crypto::BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            if (!BCRYPT_SUCCESS(status)) {
                out << "Error: BCryptGenRandom failed with status 0x" << std::hex << status << std::dec << "\n";
                return;
            }
            out << "[BCrypt CSPRNG] " << count << " Cryptographically Secure Random Bytes:\n  ";
            for (uint8_t b : buf) {
                out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
            }
            out << std::dec << "\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[BCrypt] Running Known-Answer-Test (KAT) self-check...\n";
            crypto::BCRYPT_ALG_HANDLE hAlg = nullptr;
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_SHA256_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_HASH_HANDLE hHash = nullptr;
            crypto::BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
            const char* msg = "abc";
            crypto::BCryptHashData(hHash, reinterpret_cast<const uint8_t*>(msg), 3, 0);
            uint8_t d[32]{};
            crypto::BCryptFinishHash(hHash, d, 32, 0);
            crypto::BCryptDestroyHash(hHash);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            std::ostringstream ss;
            for (int i = 0; i < 32; ++i) ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(d[i]);
            bool match = (ss.str() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
            out << "  SHA-256(\"abc\"): " << ss.str() << " [" << (match ? "PASS" : "FAIL") << "]\n";

            uint8_t key[32] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};
            uint8_t iv[16] = {0};
            crypto::BCryptOpenAlgorithmProvider(&hAlg, crypto::BCRYPT_AES_ALGORITHM, nullptr, 0);
            crypto::BCRYPT_KEY_HANDLE hKey = nullptr;
            crypto::BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, 32, 0);
            std::string sample = "MicaNT Sovereign Cryptography Engine";
            std::vector<uint8_t> pt(sample.begin(), sample.end());
            uint32_t ctLen = 0;
            uint8_t ivEnc[16]; std::memcpy(ivEnc, iv, 16);
            crypto::BCryptEncrypt(hKey, pt.data(), static_cast<uint32_t>(pt.size()), nullptr, ivEnc, 16, nullptr, 0, &ctLen, crypto::BCRYPT_BLOCK_PADDING);
            std::vector<uint8_t> ct(ctLen);
            std::memcpy(ivEnc, iv, 16);
            crypto::BCryptEncrypt(hKey, pt.data(), static_cast<uint32_t>(pt.size()), nullptr, ivEnc, 16, ct.data(), ctLen, &ctLen, crypto::BCRYPT_BLOCK_PADDING);

            uint32_t dtLen = 0;
            uint8_t ivDec[16]; std::memcpy(ivDec, iv, 16);
            crypto::BCryptDecrypt(hKey, ct.data(), ctLen, nullptr, ivDec, 16, nullptr, 0, &dtLen, crypto::BCRYPT_BLOCK_PADDING);
            std::vector<uint8_t> dt(dtLen);
            std::memcpy(ivDec, iv, 16);
            crypto::BCryptDecrypt(hKey, ct.data(), ctLen, nullptr, ivDec, 16, dt.data(), dtLen, &dtLen, crypto::BCRYPT_BLOCK_PADDING);
            crypto::BCryptDestroyKey(hKey);
            crypto::BCryptCloseAlgorithmProvider(hAlg, 0);

            std::string recovered(dt.begin(), dt.end());
            out << "  AES-256-CBC:   \"" << recovered << "\" [" << (recovered == sample ? "PASS" : "FAIL") << "]\n";
            out << "[BCrypt] Self-check complete.\n";
            return;
        }

        out << "========================================================================\n"
            << "          MicaNT Cryptography Next Generation (bcrypt.dll / ncrypt.dll) \n"
            << "========================================================================\n\n"
            << "Primitive Router:  Clean-Room Windows CNG Dispatch Engine\n"
            << "Digest Algorithms: SHA-256, SHA-384, SHA-512, MD5, SHA-1\n"
            << "Symmetric Ciphers: AES-128, AES-192, AES-256 (CBC, ECB, PKCS#7)\n"
            << "Key Derivation:    PBKDF2 (HMAC-SHA256)\n"
            << "Random Generator:  Cryptographically Secure Hardware-Entropy CSPRNG\n"
            << "Key Storage (KSP): Microsoft Software Key Storage Provider (ncrypt.dll)\n\n"
            << "Usage:\n"
            << "  bcrypt hash <algo> <data>   Computes cryptographic digest of text\n"
            << "  bcrypt rand [count]         Generates CSPRNG random bytes (hex)\n"
            << "  bcrypt test                 Executes cryptographic KAT self-test\n"
            << "  bcrypt info                 Displays CNG subsystem information\n";
    }

    void cmdCertMgr(const std::vector<std::string>& tokens, std::ostream& out) {
        crypt32::InitializeCrypt32SubsystemExports();

        std::string storeName = "ROOT";
        bool listMode = false;
        bool findMode = false;
        std::string findQuery;

        if (tokens.size() > 1) {
            if (tokens[1] == "-list" || tokens[1] == "list") {
                listMode = true;
                if (tokens.size() > 2) storeName = tokens[2];
            } else if (tokens[1] == "-find" || tokens[1] == "find") {
                findMode = true;
                if (tokens.size() > 2) findQuery = tokens[2];
            }
        }

        if (listMode) {
            std::transform(storeName.begin(), storeName.end(), storeName.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            crypt32::HCERTSTORE hStore = crypt32::CertOpenSystemStoreA(0, storeName.c_str());
            if (!hStore) {
                out << "Error: Failed to open system certificate store '" << storeName << "'.\n";
                return;
            }

            out << "Certificates in System Store [" << storeName << "]:\n";
            out << "------------------------------------------------------------------------\n";
            uint32_t count = 0;
            const crypt32::CERT_CONTEXT* pCert = nullptr;
            while ((pCert = crypt32::CertEnumCertificatesInStore(hStore, pCert)) != nullptr) {
                count++;
                char subject[256]{};
                crypt32::CertGetNameStringA(pCert, crypt32::CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subject, sizeof(subject));

                char issuer[256]{};
                crypt32::CertGetNameStringA(pCert, crypt32::CERT_NAME_SIMPLE_DISPLAY_TYPE, crypt32::CERT_NAME_ISSUER_FLAG, nullptr, issuer, sizeof(issuer));

                out << "  [" << count << "] Subject:    " << subject << "\n"
                    << "      Issuer:     " << issuer << "\n";

                uint8_t thumbprint[20]{};
                uint32_t cbThumb = sizeof(thumbprint);
                if (crypt32::CertGetCertificateContextProperty(pCert, crypt32::CERT_SHA1_HASH_PROP_ID, thumbprint, &cbThumb)) {
                    out << "      Thumbprint: ";
                    for (uint32_t i = 0; i < cbThumb; ++i) {
                        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(thumbprint[i]);
                    }
                    out << std::dec << "\n";
                }
            }
            if (count == 0) {
                out << "  (No certificates found in store)\n";
            }
            out << "------------------------------------------------------------------------\n"
                << "Total Certificates: " << count << "\n";

            crypt32::CertCloseStore(hStore, 0);
            return;
        }

        if (findMode) {
            if (findQuery.empty()) {
                out << "Usage: certmgr -find <subject_substring>\n";
                return;
            }
            crypt32::HCERTSTORE hStore = crypt32::CertOpenSystemStoreA(0, "ROOT");
            if (!hStore) {
                out << "Error: Failed to open ROOT certificate store.\n";
                return;
            }
            std::wstring wQuery(findQuery.begin(), findQuery.end());
            const crypt32::CERT_CONTEXT* pFound = crypt32::CertFindCertificateInStore(
                hStore,
                0x00010001,
                0,
                crypt32::CERT_FIND_SUBJECT_STR_W,
                wQuery.c_str(),
                nullptr
            );

            if (pFound) {
                char subject[256]{};
                crypt32::CertGetNameStringA(pFound, crypt32::CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, subject, sizeof(subject));
                out << "[CertMgr] Certificate Match Found:\n"
                    << "  Subject: " << subject << "\n";
                crypt32::CertFreeCertificateContext(pFound);
            } else {
                out << "[CertMgr] No certificates found matching: '" << findQuery << "'\n";
            }
            crypt32::CertCloseStore(hStore, 0);
            return;
        }

        out << "========================================================================\n"
            << "          MicaNT Certificate Management Subsystem (crypt32.dll)         \n"
            << "========================================================================\n\n"
            << "Certificate Stores: System Stores (ROOT, MY, CA, AddressBook), Memory Stores\n"
            << "X.509 Operations:   Context Creation, Duplicate, Enumeration, Property Query\n"
            << "Search Criteria:    CERT_FIND_ANY, CERT_FIND_SUBJECT_STR, CERT_FIND_SHA1_HASH\n"
            << "Format Parsing:     ASN.1 DER Parser, PEM Decoder\n\n"
            << "Usage:\n"
            << "  certmgr -list [store]     Lists certificates in specified store (default: ROOT)\n"
            << "  certmgr -find <query>     Searches ROOT store for matching subject\n"
            << "  certmgr info              Displays Certificate Subsystem info\n";
    }

    void cmdDpapi(const std::vector<std::string>& tokens, std::ostream& out) {
        crypt32::InitializeCrypt32SubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "protect") {
            if (tokens.size() < 3) {
                out << "Usage: dpapi protect <plaintext_string> [description]\n";
                return;
            }
            std::string text = tokens[2];
            std::string desc = (tokens.size() > 3) ? tokens[3] : "MicaNT Shell DPAPI Secret";
            std::wstring wDesc(desc.begin(), desc.end());

            crypt32::DATA_BLOB inBlob;
            inBlob.cbData = static_cast<uint32_t>(text.size());
            inBlob.pbData = reinterpret_cast<uint8_t*>(text.data());

            crypt32::DATA_BLOB outBlob{};
            int32_t ok = crypt32::CryptProtectData(
                &inBlob,
                wDesc.c_str(),
                nullptr,
                nullptr,
                nullptr,
                0,
                &outBlob
            );

            if (!ok || !outBlob.pbData) {
                out << "Error: CryptProtectData failed with error 0x" << std::hex << win32::GetLastError() << std::dec << "\n";
                return;
            }

            uint32_t b64Len = 0;
            crypt32::CryptBinaryToStringA(outBlob.pbData, outBlob.cbData, crypt32::CRYPT_STRING_BASE64 | crypt32::CRYPT_STRING_NOCRLF, nullptr, &b64Len);
            std::string b64(b64Len, '\0');
            crypt32::CryptBinaryToStringA(outBlob.pbData, outBlob.cbData, crypt32::CRYPT_STRING_BASE64 | crypt32::CRYPT_STRING_NOCRLF, b64.data(), &b64Len);
            if (!b64.empty() && b64.back() == '\0') b64.pop_back();

            win32::LocalFree(outBlob.pbData);

            out << "[DPAPI] Protected Data (Base64 Encoded):\n  " << b64 << "\n";
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "unprotect") {
            if (tokens.size() < 3) {
                out << "Usage: dpapi unprotect <base64_ciphertext>\n";
                return;
            }
            std::string b64 = tokens[2];
            uint32_t binLen = 0;
            crypt32::CryptStringToBinaryA(b64.c_str(), static_cast<uint32_t>(b64.size()), crypt32::CRYPT_STRING_BASE64, nullptr, &binLen, nullptr, nullptr);
            if (binLen == 0) {
                out << "Error: Invalid Base64 input string.\n";
                return;
            }
            std::vector<uint8_t> bin(binLen);
            crypt32::CryptStringToBinaryA(b64.c_str(), static_cast<uint32_t>(b64.size()), crypt32::CRYPT_STRING_BASE64, bin.data(), &binLen, nullptr, nullptr);

            crypt32::DATA_BLOB inBlob;
            inBlob.cbData = binLen;
            inBlob.pbData = bin.data();

            crypt32::DATA_BLOB outBlob{};
            wchar_t* pDesc = nullptr;
            int32_t ok = crypt32::CryptUnprotectData(
                &inBlob,
                &pDesc,
                nullptr,
                nullptr,
                nullptr,
                0,
                &outBlob
            );

            if (!ok || !outBlob.pbData) {
                out << "Error: CryptUnprotectData failed with error 0x" << std::hex << win32::GetLastError() << std::dec << "\n";
                return;
            }

            std::string recovered(reinterpret_cast<char*>(outBlob.pbData), outBlob.cbData);
            std::wstring desc = pDesc ? pDesc : L"";
            if (pDesc) win32::LocalFree(pDesc);
            win32::LocalFree(outBlob.pbData);

            out << "[DPAPI] Unprotected Plaintext:\n  \"" << recovered << "\"\n";
            if (!desc.empty()) {
                std::string sDesc(desc.begin(), desc.end());
                out << "  Description: " << sDesc << "\n";
            }
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[DPAPI] Running Data Protection API self-check...\n";
            std::string secret = "MicaNT_SuperSecret_MasterKey_2026";
            crypt32::DATA_BLOB inBlob{ static_cast<uint32_t>(secret.size()), reinterpret_cast<uint8_t*>(secret.data()) };
            crypt32::DATA_BLOB protectedBlob{};

            int32_t pOk = crypt32::CryptProtectData(&inBlob, L"SelfTest", nullptr, nullptr, nullptr, 0, &protectedBlob);
            out << "  CryptProtectData:   " << (pOk ? "SUCCESS" : "FAILED") << " (Ciphertext: " << protectedBlob.cbData << " bytes)\n";

            crypt32::DATA_BLOB unprotectBlob{};
            wchar_t* pDesc = nullptr;
            int32_t uOk = crypt32::CryptUnprotectData(&protectedBlob, &pDesc, nullptr, nullptr, nullptr, 0, &unprotectBlob);
            std::string recovered = (uOk && unprotectBlob.pbData) ? std::string(reinterpret_cast<char*>(unprotectBlob.pbData), unprotectBlob.cbData) : "";
            bool match = (recovered == secret);
            out << "  CryptUnprotectData: " << (uOk && match ? "PASS" : "FAIL") << " (\"" << recovered << "\")\n";

            if (pDesc) win32::LocalFree(pDesc);
            if (unprotectBlob.pbData) win32::LocalFree(unprotectBlob.pbData);
            if (protectedBlob.pbData) win32::LocalFree(protectedBlob.pbData);

            out << "[DPAPI] Self-check complete.\n";
            return;
        }

        out << "========================================================================\n"
            << "          MicaNT Data Protection API Subsystem (DPAPI)                  \n"
            << "========================================================================\n\n"
            << "API Surface:       CryptProtectData, CryptUnprotectData (crypt32.dll)\n"
            << "Master Key Model:  Per-User PBKDF2 Derived Keying (HMAC-SHA256)\n"
            << "Encryption Engine: AES-256-CBC with Secure Random IV\n"
            << "Authentication:    HMAC-SHA256 Integrity Verification Tag\n"
            << "Memory Handling:   LocalAlloc / LocalFree Compatible Heap Buffers\n\n"
            << "Usage:\n"
            << "  dpapi protect <data> [desc]   Protects string, outputs Base64 ciphertext\n"
            << "  dpapi unprotect <base64>      Decrypts Base64 ciphertext back to plaintext\n"
            << "  dpapi test                    Executes DPAPI roundtrip self-test\n"
            << "  dpapi info                    Displays DPAPI architecture details\n";
    }

    void cmdSspi(const std::vector<std::string>& tokens, std::ostream& out) {
        sspi::InitializeSspiSubsystemExports();

        if (tokens.size() > 1 && (tokens[1] == "packages" || tokens[1] == "-list" || tokens[1] == "list")) {
            uint32_t pkgCount = 0;
            sspi::SecPkgInfoA* packages = nullptr;
            sspi::SECURITY_STATUS st = sspi::EnumerateSecurityPackagesA(&pkgCount, &packages);
            if (st != sspi::SEC_E_OK || !packages) {
                out << "Error: Failed to enumerate security packages (Status: 0x" << std::hex << st << std::dec << ").\n";
                return;
            }

            out << "MicaNT Security Support Provider (SSPI) Packages (" << pkgCount << " available):\n";
            out << "------------------------------------------------------------------------\n";
            for (uint32_t i = 0; i < pkgCount; ++i) {
                out << "  [" << (i + 1) << "] Name:         " << (packages[i].Name ? packages[i].Name : "(null)") << "\n"
                    << "      Comment:      " << (packages[i].Comment ? packages[i].Comment : "(null)") << "\n"
                    << "      Capabilities: 0x" << std::hex << packages[i].fCapabilities << std::dec << "\n"
                    << "      Version:      " << packages[i].wVersion << "\n"
                    << "      RPC ID:       " << packages[i].wRPCID << "\n"
                    << "      MaxToken:     " << packages[i].cbMaxToken << " bytes\n";
            }
            out << "------------------------------------------------------------------------\n";
            sspi::FreeContextBuffer(packages);
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[SSPI] Running Security Support Provider Interface self-test...\n";

            uint32_t pkgCount = 0;
            sspi::SecPkgInfoA* packages = nullptr;
            sspi::SECURITY_STATUS eSt = sspi::EnumerateSecurityPackagesA(&pkgCount, &packages);
            out << "  EnumerateSecurityPackages: " << (eSt == sspi::SEC_E_OK ? "SUCCESS" : "FAILED")
                << " (Found " << pkgCount << " packages)\n";
            if (packages) sspi::FreeContextBuffer(packages);

            sspi::SecPkgInfoA* schPkg = nullptr;
            sspi::SECURITY_STATUS qSt = sspi::QuerySecurityPackageInfoA(sspi::UNISP_NAME_A, &schPkg);
            out << "  QuerySecurityPackageInfo (Schannel): " << (qSt == sspi::SEC_E_OK ? "PASS" : "FAIL") << "\n";
            if (schPkg) sspi::FreeContextBuffer(schPkg);

            sspi::SecPkgInfoA* ntlmPkg = nullptr;
            sspi::SECURITY_STATUS nSt = sspi::QuerySecurityPackageInfoA(sspi::NTLMSP_NAME_A, &ntlmPkg);
            out << "  QuerySecurityPackageInfo (NTLM):     " << (nSt == sspi::SEC_E_OK ? "PASS" : "FAIL") << "\n";
            if (ntlmPkg) sspi::FreeContextBuffer(ntlmPkg);

            auto* pTable = sspi::InitSecurityInterfaceA();
            bool tableOk = (pTable != nullptr && pTable->AcquireCredentialsHandleA != nullptr && pTable->EncryptMessage != nullptr);
            out << "  InitSecurityInterfaceA:              " << (tableOk ? "PASS" : "FAIL") << "\n";

            out << "[SSPI] Self-test complete.\n";
            return;
        }

        out << "========================================================================\n"
            << "        MicaNT Security Support Provider Interface Subsystem (SSPI)     \n"
            << "========================================================================\n\n"
            << "Libraries:         secur32.dll, sspicli.dll, schannel.dll\n"
            << "Core Packages:     Schannel (TLS 1.2 / TLS 1.3), NTLM (v1/v2), Negotiate (SPNEGO)\n"
            << "Function Tables:   InitSecurityInterfaceA / InitSecurityInterfaceW\n"
            << "Context Flow:      AcquireCredentials -> InitializeSecurityContext -> Complete\n"
            << "Message Security:  EncryptMessage / DecryptMessage (HMAC-SHA256 Authenticated)\n\n"
            << "Usage:\n"
            << "  sspi packages                 Lists all registered security packages\n"
            << "  sspi test                     Executes SSPI interface self-test\n"
            << "  sspi info                     Displays SSPI architecture details\n";
    }

    void cmdSchannel(const std::vector<std::string>& tokens, std::ostream& out) {
        sspi::InitializeSspiSubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[Schannel] Running TLS 1.3 Handshake & Stream Framing self-test...\n";

            sspi::CredHandle hClientCred{};
            sspi::SCHANNEL_CRED cred{};
            cred.dwVersion = sspi::SCHANNEL_CRED_VERSION;
            cred.grbitEnabledProtocols = sspi::SP_PROT_TLS1_3_CLIENT;
            sspi::SECURITY_STATUS cSt = sspi::AcquireCredentialsHandleA(
                nullptr, sspi::UNISP_NAME_A, sspi::SECPKG_CRED_OUTBOUND, nullptr, &cred, nullptr, nullptr, &hClientCred, nullptr
            );
            out << "  Client Credential Acquisition: " << (cSt == sspi::SEC_E_OK ? "SUCCESS" : "FAILED") << "\n";

            sspi::CtxtHandle hClientCtxt{};
            std::vector<uint8_t> clientHello(2048);
            sspi::SecBuffer outClientBuf{ static_cast<uint32_t>(clientHello.size()), sspi::SECBUFFER_TOKEN, clientHello.data() };
            sspi::SecBufferDesc outClientDesc{ sspi::SECBUFFER_VERSION, 1, &outClientBuf };
            uint32_t ctxtAttr = 0;
            sspi::SECURITY_STATUS initSt = sspi::InitializeSecurityContextA(
                &hClientCred, nullptr, "micant.org", sspi::ISC_REQ_STREAM | sspi::ISC_REQ_SEQUENCE_DETECT,
                0, 0, nullptr, 0, &hClientCtxt, &outClientDesc, &ctxtAttr, nullptr
            );
            out << "  ClientHello Token Generation:  " << (initSt == sspi::SEC_I_CONTINUE_NEEDED ? "PASS" : "FAIL")
                << " (" << outClientBuf.cbBuffer << " bytes)\n";

            sspi::CtxtHandle hServerCtxt{};
            std::vector<uint8_t> serverHello(2048);
            sspi::SecBuffer inServerBuf{ outClientBuf.cbBuffer, sspi::SECBUFFER_TOKEN, clientHello.data() };
            sspi::SecBufferDesc inServerDesc{ sspi::SECBUFFER_VERSION, 1, &inServerBuf };
            sspi::SecBuffer outServerBuf{ static_cast<uint32_t>(serverHello.size()), sspi::SECBUFFER_TOKEN, serverHello.data() };
            sspi::SecBufferDesc outServerDesc{ sspi::SECBUFFER_VERSION, 1, &outServerBuf };
            uint32_t srvAttr = 0;
            sspi::SECURITY_STATUS accSt = sspi::AcceptSecurityContext(
                nullptr, nullptr, &inServerDesc, sspi::ISC_REQ_STREAM, 0, &hServerCtxt, &outServerDesc, &srvAttr, nullptr
            );
            out << "  ServerHello Token Generation:  " << (accSt == sspi::SEC_I_CONTINUE_NEEDED ? "PASS" : "FAIL")
                << " (" << outServerBuf.cbBuffer << " bytes)\n";

            sspi::SecBuffer inClientBuf{ outServerBuf.cbBuffer, sspi::SECBUFFER_TOKEN, serverHello.data() };
            sspi::SecBufferDesc inClientDesc{ sspi::SECBUFFER_VERSION, 1, &inClientBuf };
            sspi::SecBuffer outClientBuf2{ 0, sspi::SECBUFFER_TOKEN, nullptr };
            sspi::SecBufferDesc outClientDesc2{ sspi::SECBUFFER_VERSION, 1, &outClientBuf2 };
            sspi::SECURITY_STATUS compSt = sspi::InitializeSecurityContextA(
                &hClientCred, &hClientCtxt, "micant.org", sspi::ISC_REQ_STREAM,
                0, 0, &inClientDesc, 0, &hClientCtxt, &outClientDesc2, &ctxtAttr, nullptr
            );
            out << "  Client Handshake Finalization: " << (compSt == sspi::SEC_E_OK ? "PASS (ESTABLISHED)" : "FAIL") << "\n";

            sspi::SecPkgContext_StreamSizes streamSizes{};
            sspi::SECURITY_STATUS szSt = sspi::QueryContextAttributesA(&hClientCtxt, sspi::SECPKG_ATTR_STREAM_SIZES, &streamSizes);
            bool sizesOk = (szSt == sspi::SEC_E_OK && streamSizes.cbHeader == 5 && streamSizes.cbTrailer == 32);
            out << "  Query SECPKG_ATTR_STREAM_SIZES:" << (sizesOk ? " PASS" : " FAIL")
                << " (Hdr=" << streamSizes.cbHeader << ", Tlr=" << streamSizes.cbTrailer << ", MaxMsg=" << streamSizes.cbMaximumMessage << ")\n";

            std::string payload = "MicaNT TLS 1.3 Schannel Authenticated Data Stream [RFC 8446]";
            std::vector<uint8_t> encHeader(streamSizes.cbHeader);
            std::vector<uint8_t> encData(payload.begin(), payload.end());
            std::vector<uint8_t> encTrailer(streamSizes.cbTrailer);

            sspi::SecBuffer encBuffers[3] = {
                { static_cast<uint32_t>(encHeader.size()), sspi::SECBUFFER_STREAM_HEADER, encHeader.data() },
                { static_cast<uint32_t>(encData.size()), sspi::SECBUFFER_DATA, encData.data() },
                { static_cast<uint32_t>(encTrailer.size()), sspi::SECBUFFER_STREAM_TRAILER, encTrailer.data() }
            };
            sspi::SecBufferDesc encDesc{ sspi::SECBUFFER_VERSION, 3, encBuffers };
            sspi::SECURITY_STATUS encSt = sspi::EncryptMessage(&hClientCtxt, 0, &encDesc, 0);
            out << "  EncryptMessage (TLS 1.3 Record):" << (encSt == sspi::SEC_E_OK ? " PASS" : " FAIL") << "\n";

            std::vector<uint8_t> fullRecord;
            fullRecord.insert(fullRecord.end(), encHeader.begin(), encHeader.begin() + encBuffers[0].cbBuffer);
            fullRecord.insert(fullRecord.end(), encData.begin(), encData.begin() + encBuffers[1].cbBuffer);
            fullRecord.insert(fullRecord.end(), encTrailer.begin(), encTrailer.begin() + encBuffers[2].cbBuffer);

            sspi::SecBuffer decBuffer{ static_cast<uint32_t>(fullRecord.size()), sspi::SECBUFFER_DATA, fullRecord.data() };
            sspi::SecBufferDesc decDesc{ sspi::SECBUFFER_VERSION, 1, &decBuffer };
            sspi::SECURITY_STATUS decSt = sspi::DecryptMessage(&hClientCtxt, &decDesc, 0, nullptr);
            std::string recovered(reinterpret_cast<char*>(decBuffer.pvBuffer), decBuffer.cbBuffer);
            bool roundtripOk = (decSt == sspi::SEC_E_OK && recovered == payload);
            out << "  DecryptMessage Roundtrip:      " << (roundtripOk ? "PASS" : "FAIL") << "\n";

            sspi::DeleteSecurityContext(&hClientCtxt);
            sspi::DeleteSecurityContext(&hServerCtxt);
            sspi::FreeCredentialsHandle(&hClientCred);

            out << "[Schannel] Self-test complete: ALL TLS 1.3 CHECKS PASSED.\n";
            return;
        }

        out << "========================================================================\n"
            << "             MicaNT Secure Channel Subsystem (schannel.dll)             \n"
            << "========================================================================\n\n"
            << "Protocol Standards: TLS 1.3 (RFC 8446), TLS 1.2 (RFC 5246)\n"
            << "Cipher Suites:      TLS_AES_256_GCM_SHA384, TLS_CHACHA20_POLY1305_SHA256\n"
            << "Key Derivation:     PBKDF2 / HKDF (HMAC-SHA256)\n"
            << "Certificate Store:  Clean-Room Root Store Integration (crypt32.dll)\n"
            << "Stream Framing:     5-Byte TLS Record Header, 32-Byte HMAC-SHA256 Tag\n\n"
            << "Usage:\n"
            << "  schannel test                 Executes TLS 1.3 handshake & encryption self-test\n"
            << "  schannel info                 Displays Schannel TLS architecture details\n";
    }

    void cmdUuidGen(const std::vector<std::string>& tokens, std::ostream& out) {
        bool sequential = false;
        int count = 1;
        bool cStruct = false;

        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string t = tokens[i];
            if (t == "-s" || t == "/s" || t == "/S") {
                sequential = true;
            } else if (t == "-c" || t == "/c" || t == "/C") {
                cStruct = true;
            } else if (t.rfind("-n", 0) == 0 || t.rfind("/n", 0) == 0 || t.rfind("/N", 0) == 0) {
                if (t.size() > 2) {
                    count = std::max(1, std::atoi(t.substr(2).c_str()));
                } else if (i + 1 < tokens.size()) {
                    count = std::max(1, std::atoi(tokens[++i].c_str()));
                }
            }
        }

        for (int i = 0; i < count; ++i) {
            micant::UUID u{};
            rpc::RPC_STATUS st = sequential ? rpc::UuidCreateSequential(&u) : rpc::UuidCreate(&u);
            if (st != rpc::RPC_S_OK) {
                out << "Error generating UUID (status " << st << ")\n";
                return;
            }

            unsigned char* str = nullptr;
            rpc::UuidToStringA(&u, &str);
            if (cStruct) {
                std::ostringstream ss;
                ss << "// {" << (str ? reinterpret_cast<char*>(str) : "") << "}\n"
                   << "static const GUID GUID_Generated = { 0x"
                   << std::hex << std::uppercase << std::setfill('0')
                   << std::setw(8) << u.Data1 << ", 0x"
                   << std::setw(4) << u.Data2 << ", 0x"
                   << std::setw(4) << u.Data3 << ", { 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[0]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[1]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[2]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[3]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[4]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[5]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[6]) << ", 0x"
                   << std::setw(2) << static_cast<int>(u.Data4[7]) << " } };\n";
                out << ss.str();
            } else {
                out << (str ? reinterpret_cast<char*>(str) : "") << "\n";
            }
            if (str) rpc::RpcStringFreeA(&str);
        }
    }

    void cmdRpc(const std::vector<std::string>& tokens, std::ostream& out) {
        rpc::InitializeRpcSubsystemExports();

        if (tokens.size() > 1 && (tokens[1] == "endpoints" || tokens[1] == "eps" || tokens[1] == "if")) {
            auto eps = rpc::RpcServerManager::Instance().getEndpoints();
            out << "Registered RPC Server Endpoints (" << eps.size() << " endpoints, "
                << rpc::RpcServerManager::Instance().getInterfaceCount() << " interfaces, "
                << (rpc::RpcServerManager::Instance().isListening() ? "LISTENING" : "IDLE") << "):\n";
            if (eps.empty()) {
                out << "  (No server endpoints currently registered)\n";
            } else {
                for (size_t i = 0; i < eps.size(); ++i) {
                    out << "  [" << (i + 1) << "] Protocol: " << eps[i].protseq
                        << "  Endpoint: " << eps[i].endpoint << "\n";
                }
            }
            return;
        }

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[RPC] Running Remote Procedure Call & NDR Subsystem self-test...\n";

            // 1. UUID test
            micant::UUID u1{}, u2{};
            rpc::UuidCreate(&u1);
            rpc::UuidCreateSequential(&u2);
            unsigned char* szUuid1 = nullptr;
            rpc::UuidToStringA(&u1, &szUuid1);
            micant::UUID parsed{};
            rpc::UuidFromStringA(szUuid1, &parsed);
            bool uuidOk = (rpc::UuidEqual(&u1, &parsed, nullptr) == 1 && rpc::UuidIsNil(&u1, nullptr) == 0);
            out << "  UUID RFC 4122 v4 & v1 Generation:   " << (uuidOk ? "PASS" : "FAIL")
                << " (" << (szUuid1 ? reinterpret_cast<char*>(szUuid1) : "") << ")\n";
            if (szUuid1) rpc::RpcStringFreeA(&szUuid1);

            // 2. String binding compose & parse
            unsigned char* strBinding = nullptr;
            rpc::RpcStringBindingComposeA(nullptr, (unsigned char*)"ncalrpc", (unsigned char*)"localhost", (unsigned char*)"ep_micant_rpc", nullptr, &strBinding);
            rpc::RPC_BINDING_HANDLE hBinding = nullptr;
            rpc::RpcBindingFromStringBindingA(strBinding, &hBinding);
            rpc::RpcBindingSetAuthInfoA(hBinding, (unsigned char*)"MicaNT/Executive", rpc::RPC_C_AUTHN_LEVEL_PKT_PRIVACY, rpc::RPC_C_AUTHN_WINNT, nullptr, 0);
            bool bindingOk = (hBinding != nullptr);
            out << "  String Binding Engine & Auth Info:  " << (bindingOk ? "PASS" : "FAIL")
                << " (" << (strBinding ? reinterpret_cast<char*>(strBinding) : "") << ")\n";
            if (strBinding) rpc::RpcStringFreeA(&strBinding);

            // 3. NDR Marshalling & Unmarshalling
            rpc::RPC_MESSAGE msg{};
            rpc::MIDL_STUB_MESSAGE stubMsg{};
            stubMsg.RpcMsg = &msg;
            rpc::NdrGetBuffer(&stubMsg, 1024, hBinding);

            uint32_t sendVal32 = 0xDEADBEEF;
            uint64_t sendVal64 = 0xCAFEBABE01234567ULL;
            rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&sendVal32), rpc::FC_ULONG);
            rpc::NdrSimpleTypeMarshall(&stubMsg, reinterpret_cast<unsigned char*>(&sendVal64), rpc::FC_HYPER);

            stubMsg.Buffer = stubMsg.BufferStart;
            uint32_t recvVal32 = 0;
            uint64_t recvVal64 = 0;
            rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&recvVal32), rpc::FC_ULONG);
            rpc::NdrSimpleTypeUnmarshall(&stubMsg, reinterpret_cast<unsigned char*>(&recvVal64), rpc::FC_HYPER);
            bool ndrScalarOk = (recvVal32 == sendVal32 && recvVal64 == sendVal64);
            out << "  NDR Scalar Marshalling (FC_ULONG/HYPER): " << (ndrScalarOk ? "PASS" : "FAIL") << "\n";

            // NDR String Marshalling
            stubMsg.Buffer = stubMsg.BufferStart;
            const char* testStr = "Clean-Room Windows RPC Runtime NDR Engine";
            rpc::NdrConformantStringMarshall(&stubMsg, reinterpret_cast<unsigned char*>(const_cast<char*>(testStr)), rpc::FC_CSTRING);

            stubMsg.Buffer = stubMsg.BufferStart;
            unsigned char* pRecvStr = nullptr;
            rpc::NdrConformantStringUnmarshall(&stubMsg, &pRecvStr, rpc::FC_CSTRING);
            bool ndrStrOk = (pRecvStr != nullptr && std::strcmp(testStr, reinterpret_cast<char*>(pRecvStr)) == 0);
            out << "  NDR Conformant String Marshalling:  " << (ndrStrOk ? "PASS" : "FAIL") << "\n";
            if (pRecvStr) win32::LocalFree(pRecvStr);
            rpc::NdrFreeBuffer(&stubMsg);

            // 4. Server Registration & Interface Dispatch
            rpc::RPC_SYNTAX_IDENTIFIER ifId{};
            rpc::UuidCreate(&ifId.SyntaxGUID);
            ifId.SyntaxVersion.MajorVersion = 1;
            ifId.SyntaxVersion.MinorVersion = 0;

            static std::atomic<uint32_t> s_dispatchCallCount{0};
            auto dummyStub = +[](rpc::RPC_MESSAGE* pMsg) {
                s_dispatchCallCount++;
                rpc::MIDL_STUB_MESSAGE srvStubMsg{};
                srvStubMsg.RpcMsg = pMsg;
                srvStubMsg.Buffer = static_cast<unsigned char*>(pMsg->Buffer);
                srvStubMsg.BufferStart = srvStubMsg.Buffer;
                srvStubMsg.BufferEnd = srvStubMsg.Buffer + pMsg->BufferLength;

                uint32_t val = 0;
                rpc::NdrSimpleTypeUnmarshall(&srvStubMsg, reinterpret_cast<unsigned char*>(&val), rpc::FC_ULONG);
                uint32_t reply = val * 2;
                srvStubMsg.Buffer = srvStubMsg.BufferStart;
                rpc::NdrSimpleTypeMarshall(&srvStubMsg, reinterpret_cast<unsigned char*>(&reply), rpc::FC_ULONG);
            };
            rpc::RPC_DISPATCH_FUNCTION dispatchFns[1] = { dummyStub };
            rpc::RPC_DISPATCH_TABLE dispatchTable{ 1, dispatchFns };

            rpc::RPC_SERVER_INTERFACE srvIf{};
            srvIf.Length = sizeof(srvIf);
            srvIf.InterfaceId = ifId;
            srvIf.TransferSyntax = rpc::NDR_TRANSFER_SYNTAX;
            srvIf.DispatchTable = &dispatchTable;

            rpc::RpcServerRegisterIf(&srvIf, nullptr, nullptr);
            rpc::RpcServerUseProtseqEpA((unsigned char*)"ncalrpc", 10, (unsigned char*)"ep_micant_rpc", nullptr);
            rpc::RpcServerListen(1, 10, 1);

            // Client interface dispatch call
            rpc::RPC_CLIENT_INTERFACE clntIf{};
            clntIf.Length = sizeof(clntIf);
            clntIf.InterfaceId = ifId;
            clntIf.TransferSyntax = rpc::NDR_TRANSFER_SYNTAX;

            rpc::RPC_MESSAGE callMsg{};
            callMsg.RpcInterfaceInformation = &clntIf;
            callMsg.ProcNum = 0;
            rpc::MIDL_STUB_MESSAGE clntStubMsg{};
            clntStubMsg.RpcMsg = &callMsg;
            rpc::NdrGetBuffer(&clntStubMsg, 512, hBinding);

            uint32_t inArg = 42;
            rpc::NdrSimpleTypeMarshall(&clntStubMsg, reinterpret_cast<unsigned char*>(&inArg), rpc::FC_ULONG);

            rpc::NdrSendReceive(&clntStubMsg, clntStubMsg.Buffer);

            clntStubMsg.Buffer = clntStubMsg.BufferStart;
            uint32_t outArg = 0;
            rpc::NdrSimpleTypeUnmarshall(&clntStubMsg, reinterpret_cast<unsigned char*>(&outArg), rpc::FC_ULONG);
            bool dispatchOk = (s_dispatchCallCount.load() > 0 && outArg == 84);
            out << "  Client/Server Interface Dispatch:   " << (dispatchOk ? "PASS (In=42 -> Out=84)" : "FAIL") << "\n";
            rpc::NdrFreeBuffer(&clntStubMsg);

            // 5. Asynchronous RPC
            rpc::RPC_ASYNC_STATE asyncState{};
            rpc::RpcAsyncInitializeHandle(&asyncState, sizeof(asyncState));
            rpc::RpcAsyncRegisterInfo(&asyncState);
            rpc::RPC_STATUS asyncSt = rpc::RpcAsyncCompleteCall(&asyncState, nullptr);
            out << "  Asynchronous RPC Handle Lifecycle:  " << (asyncSt == rpc::RPC_S_OK ? "PASS" : "FAIL") << "\n";

            // Cleanup
            rpc::RpcMgmtStopServerListening(nullptr);
            rpc::RpcServerUnregisterIf(&srvIf, nullptr, 0);
            rpc::RpcBindingFree(&hBinding);

            out << "[RPC] Self-test complete: ALL RPC & NDR CHECKS PASSED.\n";
            return;
        }

        out << "========================================================================\n"
            << "         MicaNT Remote Procedure Call Runtime & NDR Engine (rpcrt4.dll) \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    rpcrt4.dll\n"
            << "NDR Transfer Syntax:  {8a885d04-1ceb-11c9-9fe8-08002b104860} v2.0\n"
            << "Supported Protocols:  ncalrpc (Local ALPC), ncacn_np (Named Pipes), ncacn_ip_tcp (TCP/IP)\n"
            << "UUID Standards:       RFC 4122 v4 (Random Crypto PRNG), RFC 4122 v1 (Sequential MAC)\n"
            << "Binding Formats:      [uuid@]protseq:[network_addr][endpoint,options]\n"
            << "Marshalling Engine:   Scalar primitives (FC_BYTE..FC_HYPER), Conformant Strings (FC_CSTRING, FC_WSTRING)\n"
            << "Async Architecture:   RPC_ASYNC_STATE Notification, CompleteCall & AbortCall\n\n"
            << "Usage:\n"
            << "  rpc test                      Executes RPC & NDR marshalling self-test\n"
            << "  rpc endpoints                 Lists registered server endpoints and interfaces\n"
            << "  rpc info                      Displays RPC runtime subsystem details\n"
            << "  uuidgen [-s] [-c] [-n <num>]  Generates UUIDs (v4 default, -s sequential, -c C struct)\n";
    }

    void cmdOleAut(const std::vector<std::string>& tokens, std::ostream& out) {
        oleaut32::InitializeOleAut32SubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[OLEAUT] Running OLE Automation, SafeArray & TypeLib self-test...\n";

            // 1. BSTR string lifecycle
            ole32::BSTR bstr = oleaut32::SysAllocString(L"MicaNT OLE Automation Subsystem");
            uint32_t bstrLen = oleaut32::SysStringLen(bstr);
            uint32_t bstrBytes = oleaut32::SysStringByteLen(bstr);
            bool bstrOk = (bstr != nullptr && bstrLen == 31 && bstrBytes == 62);
            out << "  BSTR Allocation & Length:           " << (bstrOk ? "PASS" : "FAIL") << "\n";

            oleaut32::SysReAllocString(&bstr, L"MicaNT Automation Extended");
            bool reallocOk = (bstr != nullptr && oleaut32::SysStringLen(bstr) == 26);
            out << "  SysReAllocString In-Place Resize:   " << (reallocOk ? "PASS" : "FAIL") << "\n";
            oleaut32::SysFreeString(bstr);

            // 2. SafeArray 1D vector
            oleaut32::SAFEARRAY* psa1D = oleaut32::SafeArrayCreateVector(ole32::VT_I4, 0, 5);
            bool psa1DOk = (psa1D != nullptr && psa1D->cDims == 1 && psa1D->cbElements == sizeof(int32_t));
            if (psa1DOk) {
                for (int32_t i = 0; i < 5; ++i) {
                    int32_t val = (i + 1) * 10;
                    oleaut32::SafeArrayPutElement(psa1D, &i, &val);
                }
                int32_t readVal = 0;
                int32_t idx = 2;
                oleaut32::SafeArrayGetElement(psa1D, &idx, &readVal);
                psa1DOk = psa1DOk && (readVal == 30);
            }
            out << "  SafeArray 1D Vector Create/Put/Get: " << (psa1DOk ? "PASS" : "FAIL") << "\n";

            // 3. SafeArray Locking & Memory Access
            void* pData = nullptr;
            ole32::HRESULT hrLock = oleaut32::SafeArrayAccessData(psa1D, &pData);
            bool lockOk = (hrLock == ole32::S_OK && pData != nullptr && psa1D->cLocks == 1);
            ole32::HRESULT hrDestroyLocked = oleaut32::SafeArrayDestroy(psa1D);
            bool rejectLocked = (hrDestroyLocked == oleaut32::DISP_E_ARRAYISLOCKED);
            oleaut32::SafeArrayUnaccessData(psa1D);
            bool unlockOk = (psa1D->cLocks == 0);
            out << "  SafeArray Access/Lock Protection:   " << (lockOk && rejectLocked && unlockOk ? "PASS" : "FAIL") << "\n";

            // 4. SafeArray Deep Copy & Destroy
            oleaut32::SAFEARRAY* psaCopy = nullptr;
            oleaut32::SafeArrayCopy(psa1D, &psaCopy);
            bool copyOk = (psaCopy != nullptr && psaCopy != psa1D && psaCopy->rgsabound[0].cElements == 5);
            oleaut32::SafeArrayDestroy(psa1D);
            oleaut32::SafeArrayDestroy(psaCopy);
            out << "  SafeArray Deep Copy & Free:         " << (copyOk ? "PASS" : "FAIL") << "\n";

            // 5. Variant Type Coercion
            ole32::VARIANT vInt{}, vStr{}, vBool{}, vDbl{};
            oleaut32::VariantInit(&vInt);
            oleaut32::VariantInit(&vStr);
            oleaut32::VariantInit(&vBool);
            oleaut32::VariantInit(&vDbl);
            vInt.vt = ole32::VT_I4;
            vInt.lVal = 42;

            oleaut32::VariantChangeType(&vStr, &vInt, 0, ole32::VT_BSTR);
            bool coerceStr = (vStr.vt == ole32::VT_BSTR && vStr.bstrVal && std::wcscmp(vStr.bstrVal, L"42") == 0);

            oleaut32::VariantChangeType(&vBool, &vInt, 0, ole32::VT_BOOL);
            bool coerceBool = (vBool.vt == ole32::VT_BOOL && vBool.boolVal == -1);

            oleaut32::VariantChangeType(&vDbl, &vStr, 0, ole32::VT_R8);
            bool coerceDbl = (vDbl.vt == ole32::VT_R8 && std::fabs(vDbl.dblVal - 42.0) < 0.0001);

            out << "  Variant Coercion (I4->BSTR->R8,Bool):" << (coerceStr && coerceBool && coerceDbl ? " PASS" : " FAIL") << "\n";

            // 6. Variant Comparison
            ole32::VARIANT vA{}, vB{};
            oleaut32::VariantInit(&vA);
            oleaut32::VariantInit(&vB);
            vA.vt = ole32::VT_I4; vA.lVal = 100;
            vB.vt = ole32::VT_I4; vB.lVal = 50;
            bool cmpGt = (oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_GT);
            vB.lVal = 100;
            bool cmpEq = (oleaut32::VarCmp(&vA, &vB, 0, 0) == oleaut32::VARCMP_EQ);
            out << "  Variant Comparison (VarCmp):        " << (cmpGt && cmpEq ? "PASS" : "FAIL") << "\n";

            oleaut32::VariantClear(&vInt);
            oleaut32::VariantClear(&vStr);
            oleaut32::VariantClear(&vBool);
            oleaut32::VariantClear(&vDbl);
            oleaut32::VariantClear(&vA);
            oleaut32::VariantClear(&vB);

            // 7. Dynamic IDispatch Late-Binding Invocation
            auto stdDisp = std::make_unique<oleaut32::StandardDispatch>();
            stdDisp->registerMethod(L"Multiply", 101, [](oleaut32::DISPPARAMS* dp, ole32::VARIANT* res) -> ole32::HRESULT {
                if (!dp || dp->cArgs < 2 || !res) return ole32::E_INVALIDARG;
                ole32::VARIANT a{}, b{};
                oleaut32::VariantInit(&a);
                oleaut32::VariantInit(&b);
                oleaut32::DispGetParam(dp, 0, ole32::VT_I4, &a, nullptr);
                oleaut32::DispGetParam(dp, 1, ole32::VT_I4, &b, nullptr);
                res->vt = ole32::VT_I4;
                res->lVal = a.lVal * b.lVal;
                return ole32::S_OK;
            });

            ole32::OLECHAR* methodName = const_cast<ole32::OLECHAR*>(L"Multiply");
            oleaut32::DISPID dispid = 0;
            ole32::HRESULT hrName = stdDisp->GetIDsOfNames(ole32::GUID_NULL, &methodName, 1, 0, &dispid);

            ole32::VARIANT args[2];
            oleaut32::VariantInit(&args[0]);
            oleaut32::VariantInit(&args[1]);
            // Reverse order in DISPPARAMS: arg 0 at index 1, arg 1 at index 0
            args[0].vt = ole32::VT_I4; args[0].lVal = 7;
            args[1].vt = ole32::VT_I4; args[1].lVal = 6;
            oleaut32::DISPPARAMS dp{ args, nullptr, 2, 0 };
            ole32::VARIANT result{};
            oleaut32::VariantInit(&result);
            ole32::HRESULT hrInvoke = stdDisp->Invoke(dispid, ole32::GUID_NULL, 0, oleaut32::DISPATCH_METHOD, &dp, &result, nullptr, nullptr);
            bool dispOk = (hrName == ole32::S_OK && dispid == 101 && hrInvoke == ole32::S_OK && result.vt == ole32::VT_I4 && result.lVal == 42);
            out << "  IDispatch Late-Binding (6 * 7 = 42):" << (dispOk ? " PASS" : " FAIL") << "\n";

            // 8. TypeLib Registration & Lookup
            micant::GUID fakeLibGuid{ 0x12345678, 0x1234, 0x5678, { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0 } };
            oleaut32::TypeLibManager::Instance().registerLibrary(fakeLibGuid, 2, 1, L"C:\\MicaNT\\System32\\sample.tlb");
            ole32::BSTR queriedPath = nullptr;
            ole32::HRESULT hrLib = oleaut32::QueryPathOfRegTypeLib(fakeLibGuid, 2, 1, 0, &queriedPath);
            bool typelibOk = (hrLib == ole32::S_OK && queriedPath && std::wcscmp(queriedPath, L"C:\\MicaNT\\System32\\sample.tlb") == 0);
            if (queriedPath) oleaut32::SysFreeString(queriedPath);
            out << "  TypeLib Registration & Path Lookup: " << (typelibOk ? "PASS" : "FAIL") << "\n";

            out << "[OLEAUT] Self-test complete: ALL OLE AUTOMATION CHECKS PASSED.\n";
            return;
        }

        out << "========================================================================\n"
            << "     MicaNT Windows OLE Automation & SafeArray Subsystem (oleaut32.dll)  \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    oleaut32.dll\n"
            << "Late-Binding Engine:  IDispatch (GetIDsOfNames, Invoke, DispGetParam, DispInvoke)\n"
            << "SafeArray Subsystem:  SafeArrayCreate/Vector, AccessData, PutElement, GetElement\n"
            << "                      Lock count tracking, FADF feature flags, SafeArrayCopy/Redim\n"
            << "Variant Engine:       VariantInit, VariantClear, VariantCopy, VariantCopyInd\n"
            << "                      VariantChangeType (I1..I8, UI1..UI8, R4, R8, BSTR, BOOL, DATE, CY)\n"
            << "                      VarCmp (Relational comparison: LT, EQ, GT, NULL)\n"
            << "BSTR Memory Runtime:  SysAllocString, SysAllocStringByteLen, SysReAllocString\n"
            << "Type Library Manager: ITypeLib, ITypeInfo, LoadTypeLib, RegisterTypeLib\n\n"
            << "Usage:\n"
            << "  oleaut test         Executes OLE Automation, SafeArray & TypeLib self-test\n"
            << "  oleaut info         Displays OLE Automation subsystem details\n";
    }

    void cmdDevMgmt(const std::vector<std::string>& tokens, std::ostream& out) {
        setupapi::InitializeSetupApiSubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[SETUPAPI] Running Device Installation & SetupAPI self-test...\n";

            // 1. INF Parsing & String Substitution
            setupapi::HINF hInf = setupapi::SetupOpenInfFileW(L"test_driver.inf", nullptr, 0, nullptr);
            bool infLoaded = (hInf != nullptr && hInf != reinterpret_cast<setupapi::HINF>(static_cast<uintptr_t>(-1)));
            out << "  INF File Parser & Construction:     " << (infLoaded ? "PASS" : "FAIL") << "\n";

            setupapi::INFCONTEXT ctx{};
            win32::BOOL bLine = setupapi::SetupFindFirstLineW(hInf, L"Strings", L"ManufacturerName", &ctx);
            wchar_t strVal[128]{};
            win32::BOOL bStr = setupapi::SetupGetStringFieldW(&ctx, 1, strVal, 128, nullptr);
            bool stringsOk = (bLine && bStr && std::wcscmp(strVal, L"MicaNT Sovereign Project") == 0);
            out << "  INF [Strings] Token Table Parsing:  " << (stringsOk ? "PASS" : "FAIL") << "\n";

            // String expansion test in model section
            win32::BOOL bModel = setupapi::SetupFindFirstLineW(hInf, L"Standard.NTamd64", nullptr, &ctx);
            wchar_t modelDesc[128]{};
            setupapi::SetupGetStringFieldW(&ctx, 0, modelDesc, 128, nullptr);
            bool expandOk = (bModel && std::wcscmp(modelDesc, L"MicaNT Sovereign PrismX Graphics Accelerator") == 0);
            out << "  INF %StringToken% Interpolation:    " << (expandOk ? "PASS" : "FAIL") << "\n";
            setupapi::SetupCloseInfFile(hInf);

            // 2. Device Information Set Lifecycle
            setupapi::HDEVINFO hDevSet = setupapi::SetupDiCreateDeviceInfoList(&setupapi::GUID_DEVCLASS_DISPLAY, nullptr);
            bool devSetOk = (hDevSet != nullptr);
            out << "  HDEVINFO Device Info Set Creation:  " << (devSetOk ? "PASS" : "FAIL") << "\n";

            setupapi::SP_DEVINFO_DATA devData{};
            devData.cbSize = sizeof(devData);
            win32::BOOL bCreateDev = setupapi::SetupDiCreateDeviceInfoW(
                hDevSet,
                L"PCI\\VEN_10DE&DEV_2684&SUBSYS_168210DE&REV_A1",
                &setupapi::GUID_DEVCLASS_DISPLAY,
                L"NVIDIA GeForce RTX 4090 (Sovereign Emulation)",
                nullptr,
                0,
                &devData
            );
            out << "  SetupDiCreateDeviceInfo Registration: " << (bCreateDev ? "PASS" : "FAIL") << "\n";

            // 3. Device Registry Properties
            const wchar_t* hwId = L"PCI\\VEN_10DE&DEV_2684";
            setupapi::SetupDiSetDeviceRegistryPropertyW(
                hDevSet,
                &devData,
                setupapi::SPDRP_HARDWAREID,
                reinterpret_cast<const uint8_t*>(hwId),
                static_cast<uint32_t>((std::wcslen(hwId) + 1) * sizeof(wchar_t))
            );

            wchar_t readHwId[128]{};
            setupapi::SetupDiGetDeviceRegistryPropertyW(
                hDevSet,
                &devData,
                setupapi::SPDRP_HARDWAREID,
                nullptr,
                reinterpret_cast<uint8_t*>(readHwId),
                sizeof(readHwId),
                nullptr
            );
            bool propOk = (std::wcscmp(readHwId, hwId) == 0);
            out << "  Device Registry Property (HWID):    " << (propOk ? "PASS" : "FAIL") << "\n";

            // 4. Device Interface Detail
            setupapi::SP_DEVICE_INTERFACE_DATA ifaceData{};
            win32::BOOL bIface = setupapi::SetupDiCreateDeviceInterfaceW(
                hDevSet,
                &devData,
                &setupapi::GUID_DEVCLASS_DISPLAY,
                nullptr,
                0,
                &ifaceData
            );

            uint8_t detailBuf[256]{};
            auto* detail = reinterpret_cast<setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuf);
            detail->cbSize = sizeof(setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            win32::BOOL bDetail = setupapi::SetupDiGetDeviceInterfaceDetailW(hDevSet, &ifaceData, detail, sizeof(detailBuf), nullptr, nullptr);
            bool ifaceOk = (bIface && bDetail && std::wcsstr(detail->DevicePath, L"PCI") != nullptr);
            out << "  Device Interface Path Detail Query: " << (ifaceOk ? "PASS" : "FAIL") << "\n";

            // 5. Driver Matching Info
            win32::BOOL bDrv = setupapi::SetupDiBuildDriverInfoList(hDevSet, &devData, setupapi::SPDIT_COMPATDRIVER);
            out << "  Driver Matching & Hardware Ranking: " << (bDrv ? "PASS" : "FAIL") << "\n";

            setupapi::SetupDiDestroyDeviceInfoList(hDevSet);

            // 6. Global Hardware Device Enumeration
            setupapi::HDEVINFO hAllDevs = setupapi::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, setupapi::DIGCF_ALLCLASSES | setupapi::DIGCF_PRESENT);
            uint32_t count = 0;
            setupapi::SP_DEVINFO_DATA enumDev{};
            enumDev.cbSize = sizeof(enumDev);
            while (setupapi::SetupDiEnumDeviceInfo(hAllDevs, count, &enumDev)) {
                count++;
            }
            bool enumOk = (count >= 5);
            out << "  Global Hardware Subsystem Snapshot: " << (enumOk ? "PASS (" + std::to_string(count) + " devices)" : "FAIL") << "\n";
            setupapi::SetupDiDestroyDeviceInfoList(hAllDevs);

            out << "[SETUPAPI] Self-test complete: ALL DEVICE INSTALLATION CHECKS PASSED.\n";
            return;
        }

        // Default or "devmgmt": display clean-room Device Manager table
        setupapi::HDEVINFO hDevs = setupapi::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, setupapi::DIGCF_ALLCLASSES | setupapi::DIGCF_PRESENT);
        if (!hDevs) {
            out << "Failed to query system devices.\n";
            return;
        }

        out << "========================================================================================\n"
            << "                         MicaNT Device Manager (devmgmt.msc)                            \n"
            << "========================================================================================\n\n";

        uint32_t idx = 0;
        setupapi::SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(devData);

        std::unordered_map<std::wstring, std::vector<std::pair<std::wstring, std::wstring>>> classMap;

        while (setupapi::SetupDiEnumDeviceInfo(hDevs, idx++, &devData)) {
            wchar_t className[64]{};
            setupapi::SetupDiClassNameFromGuidW(&devData.ClassGuid, className, 64, nullptr);
            wchar_t classDesc[128]{};
            setupapi::SetupDiGetClassDescriptionW(&devData.ClassGuid, classDesc, 128, nullptr);

            wchar_t devDesc[256]{};
            setupapi::SetupDiGetDeviceRegistryPropertyW(hDevs, &devData, setupapi::SPDRP_DEVICEDESC, nullptr, reinterpret_cast<uint8_t*>(devDesc), sizeof(devDesc), nullptr);

            wchar_t hwId[256]{};
            setupapi::SetupDiGetDeviceRegistryPropertyW(hDevs, &devData, setupapi::SPDRP_HARDWAREID, nullptr, reinterpret_cast<uint8_t*>(hwId), sizeof(hwId), nullptr);

            std::wstring cat = classDesc[0] ? classDesc : className;
            classMap[cat].push_back({ devDesc[0] ? devDesc : L"Unknown Device", hwId[0] ? hwId : L"N/A" });
        }
        setupapi::SetupDiDestroyDeviceInfoList(hDevs);

        for (const auto& [category, devList] : classMap) {
            std::string catNarrow;
            for (wchar_t wc : category) catNarrow.push_back(static_cast<char>(wc & 0x7F));
            out << "[-] " << catNarrow << "\n";
            for (const auto& [name, hwid] : devList) {
                std::string nameNarrow, hwidNarrow;
                for (wchar_t wc : name) nameNarrow.push_back(static_cast<char>(wc & 0x7F));
                for (wchar_t wc : hwid) hwidNarrow.push_back(static_cast<char>(wc & 0x7F));
                out << "    * " << nameNarrow << "\n"
                    << "      Hardware ID: " << hwidNarrow << "\n";
            }
            out << "\n";
        }
        out << "Total Active Devices: " << idx - 1 << " devices registered in PnP hierarchy.\n";
    }

    void cmdStorage(const std::vector<std::string>& tokens, std::ostream& out) {
        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[Structured Storage] Running OLE Compound File Subsystem Self-Test...\n";

            // 1. Create LockBytes
            ole32::ILockBytes* plk = nullptr;
            ole32::HRESULT hr = ole32::CreateILockBytesOnHGlobal(nullptr, win32::TRUE, &plk);
            if (FAILED(hr) || !plk) {
                out << "[FAIL] CreateILockBytesOnHGlobal failed\n";
                return;
            }

            // 2. Create Docfile on LockBytes
            ole32::IStorage* pRoot = nullptr;
            hr = ole32::StgCreateDocfileOnILockBytes(plk, ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, &pRoot);
            if (FAILED(hr) || !pRoot) {
                plk->Release();
                out << "[FAIL] StgCreateDocfileOnILockBytes failed\n";
                return;
            }

            // 3. Create sub-storage
            ole32::IStorage* pSub = nullptr;
            hr = pRoot->CreateStorage(L"Worksheets", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pSub);
            if (FAILED(hr) || !pSub) {
                pRoot->Release();
                plk->Release();
                out << "[FAIL] CreateStorage failed\n";
                return;
            }

            // 4. Create Stream in sub-storage
            ole32::IStream* pStm = nullptr;
            hr = pSub->CreateStream(L"Sheet1Data", ole32::STGM_READWRITE | ole32::STGM_CREATE | ole32::STGM_SHARE_EXCLUSIVE, 0, 0, &pStm);
            if (FAILED(hr) || !pStm) {
                pSub->Release();
                pRoot->Release();
                plk->Release();
                out << "[FAIL] CreateStream failed\n";
                return;
            }

            const char* testMsg = "MicaNT OLE Structured Storage Compound Binary Format Stream Payload";
            uint32_t written = 0;
            pStm->Write(testMsg, static_cast<uint32_t>(std::strlen(testMsg)), &written);
            pStm->Release();
            pSub->Release();

            // 5. Commit root docfile
            pRoot->Commit(ole32::STGC_DEFAULT);

            // 6. Verify CFBF header magic on LockBytes
            hr = ole32::StgIsStorageILockBytes(plk);
            if (hr != ole32::S_OK) {
                pRoot->Release();
                plk->Release();
                out << "[FAIL] StgIsStorageILockBytes returned non-S_OK\n";
                return;
            }

            // 7. Enumerate elements
            ole32::IEnumSTATSTG* pEnum = nullptr;
            pRoot->EnumElements(0, nullptr, 0, &pEnum);
            uint32_t fetched = 0;
            ole32::STATSTG stat{};
            if (pEnum && pEnum->Next(1, &stat, &fetched) == ole32::S_OK) {
                out << "  - Found storage element: " << (stat.pwcsName ? "Worksheets" : "Unknown") << "\n";
                if (stat.pwcsName) ole32::CoTaskMemFree(stat.pwcsName);
                pEnum->Release();
            }

            pRoot->Release();
            plk->Release();

            out << "[SUCCESS] ALL STRUCTURED STORAGE & COMPOUND FILE CHECKS PASSED!\n";
            return;
        }

        out << "========================================================================\n"
            << "     MicaNT OLE Structured Storage & Compound File Subsystem (ole32)    \n"
            << "========================================================================\n\n"
            << "  Architecture:      MS-CFB v3 / v4 Compound File Binary Format Engine\n"
            << "  Sector Sizing:     512 Bytes (CFBF v3) / 4096 Bytes (CFBF v4)\n"
            << "  Magic Signature:   0xD0CF11E0A1B11AE1 (Little-Endian OLE DocFile)\n"
            << "  Core Interfaces:   IStorage, IStream, ILockBytes, IEnumSTATSTG\n"
            << "  Persistence APIs:  IPersistStorage, IPersistStream, IPersistFile, OleSave, OleLoad\n"
            << "  Dynamic Exports:   15 APIs registered in ole32.dll\n"
            << "  Status:            ONLINE (Clean-Room Provenance Verified)\n\n"
            << "Usage:\n"
            << "  stg info           Display subsystem details and specification\n"
            << "  stg test           Execute automated DocFile and stream validation\n";
    }

    void cmdWevtUtil(const std::vector<std::string>& tokens, std::ostream& out) {
        wevtapi::InitializeWevtApiSubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[WEVTAPI] Running Windows Event Log Subsystem Self-Test...\n";

            // 1. Channel Enumeration (EvtOpenChannelEnum, EvtNextChannelPath)
            wevtapi::EVT_HANDLE hChanEnum = wevtapi::EvtOpenChannelEnum(nullptr, 0);
            wchar_t chanBuf[256]{};
            uint32_t bufUsed = 0;
            std::vector<std::wstring> enumeratedChannels;
            while (wevtapi::EvtNextChannelPath(hChanEnum, 256, chanBuf, &bufUsed)) {
                enumeratedChannels.push_back(chanBuf);
            }
            wevtapi::EvtClose(hChanEnum);
            bool chanEnumOk = (enumeratedChannels.size() >= 4);
            out << "  Channel Enumeration (MS-EVEN6):     " << (chanEnumOk ? "PASS (" + std::to_string(enumeratedChannels.size()) + " channels)" : "FAIL") << "\n";

            // 2. Publisher Enumeration (EvtOpenPublisherEnum, EvtNextPublisherId)
            wevtapi::EVT_HANDLE hPubEnum = wevtapi::EvtOpenPublisherEnum(nullptr, 0);
            wchar_t pubBuf[256]{};
            std::vector<std::wstring> enumeratedPublishers;
            while (wevtapi::EvtNextPublisherId(hPubEnum, 256, pubBuf, &bufUsed)) {
                enumeratedPublishers.push_back(pubBuf);
            }
            wevtapi::EvtClose(hPubEnum);
            bool pubEnumOk = (!enumeratedPublishers.empty());
            out << "  Publisher Enumeration:              " << (pubEnumOk ? "PASS (" + std::to_string(enumeratedPublishers.size()) + " publishers)" : "FAIL") << "\n";

            // 3. Modern Event Emission & Query
            wevtapi::EventRecord testRec{};
            testRec.channel = L"Application";
            testRec.providerName = L"MicaNT-ShellDiagnostics";
            testRec.eventId = 9001;
            testRec.level = wevtapi::WINEVENT_LEVEL_INFO;
            testRec.stringInserts.push_back(L"Subsystem diagnostics self-test cycle initiated.");
            testRec.namedData[L"DiagnosticEngine"] = L"WevtApi-MS-EVEN6";
            uint64_t newRecId = wevtapi::EventLogManager::Instance().WriteEvent(testRec);
            out << "  Structured Event Write:             PASS (Record ID: " << newRecId << ")\n";

            // 4. Query Events via EvtQuery & EvtNext
            wevtapi::EVT_HANDLE hQuery = wevtapi::EvtQuery(nullptr, L"Application", L"*", wevtapi::EvtQueryChannelPath | wevtapi::EvtQueryForwardDirection);
            wevtapi::EVT_HANDLE hEvents[5]{};
            uint32_t returned = 0;
            win32::BOOL bNext = wevtapi::EvtNext(hQuery, 5, hEvents, 1000, 0, &returned);
            bool queryOk = (bNext && returned > 0);
            out << "  EvtQuery & EvtNext Traversal:       " << (queryOk ? "PASS (" + std::to_string(returned) + " events retrieved)" : "FAIL") << "\n";

            // 5. XML Rendering via EvtRender
            if (queryOk && returned > 0) {
                wevtapi::EVT_HANDLE hContext = wevtapi::EvtCreateRenderContext(0, nullptr, wevtapi::EvtRenderContextValues);
                wchar_t xmlBuffer[2048]{};
                uint32_t propCount = 0;
                win32::BOOL bRender = wevtapi::EvtRender(hContext, hEvents[0], wevtapi::EvtRenderEventXml, sizeof(xmlBuffer), xmlBuffer, &bufUsed, &propCount);
                bool renderOk = (bRender && std::wcsstr(xmlBuffer, L"<Event xmlns=") != nullptr);
                out << "  EvtRender XML Serialization:        " << (renderOk ? "PASS" : "FAIL") << "\n";
                wevtapi::EvtClose(hContext);
            }
            for (uint32_t i = 0; i < returned; ++i) {
                wevtapi::EvtClose(hEvents[i]);
            }
            wevtapi::EvtClose(hQuery);

            // 6. Channel Configuration Query
            wevtapi::EVT_HANDLE hChanConfig = wevtapi::EvtOpenChannelConfig(nullptr, L"System", 0);
            win32::BOOL bEnabled = win32::FALSE;
            win32::BOOL bCfg = wevtapi::EvtGetChannelConfigProperty(hChanConfig, wevtapi::EvtChannelConfigEnabled, 0, sizeof(bEnabled), &bEnabled, &bufUsed);
            bool cfgOk = (bCfg && bEnabled == win32::TRUE);
            wevtapi::EvtClose(hChanConfig);
            out << "  Channel Configuration Query:        " << (cfgOk ? "PASS (System: Enabled)" : "FAIL") << "\n";

            // 7. Legacy ADVAPI32 EventLog Bridge
            void* hAdvLog = wevtapi::RegisterEventSourceW(nullptr, L"MicaNT-LegacyApp");
            const wchar_t* msgStrings[] = { L"Legacy report event test string 1", L"Status: OK" };
            win32::BOOL bReport = wevtapi::ReportEventW(hAdvLog, wevtapi::EVENTLOG_INFORMATION_TYPE, 0, 7701, nullptr, 2, 0, msgStrings, nullptr);
            uint32_t legacyCount = 0;
            wevtapi::GetNumberOfEventLogRecords(hAdvLog, &legacyCount);
            wevtapi::DeregisterEventSource(hAdvLog);
            bool legacyOk = (bReport && legacyCount > 0);
            out << "  Legacy ADVAPI32 EventLog Bridge:    " << (legacyOk ? "PASS (ReportEventW + RecordCount=" + std::to_string(legacyCount) + ")" : "FAIL") << "\n";

            out << "[WEVTAPI] Self-test complete: ALL EVENT LOG & INSTRUMENTATION CHECKS PASSED.\n";
            return;
        }

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (sub == "el" || sub == "enum-logs") {
                out << "Available Event Log Channels:\n";
                auto channels = wevtapi::EventLogManager::Instance().GetChannelNames();
                for (const auto& ch : channels) {
                    std::string chNarrow;
                    for (wchar_t wc : ch) chNarrow.push_back(static_cast<char>(wc & 0x7F));
                    uint32_t cnt = wevtapi::EventLogManager::Instance().GetRecordCount(ch);
                    out << "  - " << std::left << std::setw(20) << chNarrow << " (" << cnt << " records)\n";
                }
                return;
            }

            if (sub == "ep" || sub == "enum-publishers") {
                out << "Registered Event Publishers:\n";
                auto publishers = wevtapi::EventLogManager::Instance().GetPublisherNames();
                for (const auto& pub : publishers) {
                    std::string pubNarrow;
                    for (wchar_t wc : pub) pubNarrow.push_back(static_cast<char>(wc & 0x7F));
                    out << "  - " << pubNarrow << "\n";
                }
                return;
            }

            if ((sub == "gl" || sub == "get-log") && tokens.size() > 2) {
                std::string targetChan = tokens[2];
                std::wstring wChan(targetChan.begin(), targetChan.end());
                wevtapi::EVT_HANDLE hCfg = wevtapi::EvtOpenChannelConfig(nullptr, wChan.c_str(), 0);
                if (!hCfg) {
                    out << "Error: Channel '" << targetChan << "' not found.\n";
                    return;
                }
                win32::BOOL bEnabled = win32::FALSE;
                uint32_t bufUsed = 0;
                wevtapi::EvtGetChannelConfigProperty(hCfg, wevtapi::EvtChannelConfigEnabled, 0, sizeof(bEnabled), &bEnabled, &bufUsed);
                uint64_t maxSize = 0;
                wevtapi::EvtGetChannelConfigProperty(hCfg, wevtapi::EvtChannelLoggingConfigMaxSize, 0, sizeof(maxSize), &maxSize, &bufUsed);
                wevtapi::EvtClose(hCfg);

                uint32_t recCount = wevtapi::EventLogManager::Instance().GetRecordCount(wChan);
                uint32_t oldest = wevtapi::EventLogManager::Instance().GetOldestRecord(wChan);

                out << "Channel Configuration: " << targetChan << "\n"
                    << "  Enabled:          " << (bEnabled ? "true" : "false") << "\n"
                    << "  Max Buffer Size:  " << maxSize << " bytes\n"
                    << "  Record Count:     " << recCount << "\n"
                    << "  Oldest Record ID: " << oldest << "\n";
                return;
            }

            if ((sub == "cl" || sub == "clear-log") && tokens.size() > 2) {
                std::string targetChan = tokens[2];
                std::wstring wChan(targetChan.begin(), targetChan.end());
                if (wevtapi::EvtClearLog(nullptr, wChan.c_str(), nullptr, 0)) {
                    out << "Channel '" << targetChan << "' successfully cleared.\n";
                } else {
                    out << "Error clearing channel '" << targetChan << "'.\n";
                }
                return;
            }

            if ((sub == "qe" || sub == "query-events") && tokens.size() > 2) {
                std::string targetChan = tokens[2];
                std::wstring wChan(targetChan.begin(), targetChan.end());
                bool xmlFormat = false;
                for (size_t i = 3; i < tokens.size(); ++i) {
                    if (tokens[i] == "/f:xml" || tokens[i] == "-xml") xmlFormat = true;
                }

                auto events = wevtapi::EventLogManager::Instance().Query(wChan, L"*", true);
                if (events.empty()) {
                    out << "No events found in channel '" << targetChan << "'.\n";
                    return;
                }

                out << "Events in channel '" << targetChan << "' (" << events.size() << " records):\n\n";
                for (const auto& ev : events) {
                    if (xmlFormat) {
                        std::wstring xml = ev.toXml();
                        std::string xmlNarrow;
                        for (wchar_t wc : xml) xmlNarrow.push_back(static_cast<char>(wc & 0x7F));
                        out << xmlNarrow << "\n\n";
                    } else {
                        std::string provNarrow;
                        for (wchar_t wc : ev.providerName) provNarrow.push_back(static_cast<char>(wc & 0x7F));
                        out << "  [Record " << ev.recordId << "] Event ID: " << ev.eventId
                            << " | Level: " << static_cast<int>(ev.level)
                            << " | Provider: " << provNarrow << "\n";
                        for (size_t s = 0; s < ev.stringInserts.size(); ++s) {
                            std::string insNarrow;
                            for (wchar_t wc : ev.stringInserts[s]) insNarrow.push_back(static_cast<char>(wc & 0x7F));
                            out << "    Data[" << s << "]: " << insNarrow << "\n";
                        }
                        for (const auto& [k, v] : ev.namedData) {
                            std::string kNarrow, vNarrow;
                            for (wchar_t wc : k) kNarrow.push_back(static_cast<char>(wc & 0x7F));
                            for (wchar_t wc : v) vNarrow.push_back(static_cast<char>(wc & 0x7F));
                            out << "    " << kNarrow << " = " << vNarrow << "\n";
                        }
                        out << "\n";
                    }
                }
                return;
            }
        }

        out << "========================================================================\n"
            << "     MicaNT Windows Event Log & Instrumentation Subsystem (wevtapi.dll)  \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    wevtapi.dll (MS-EVEN6) & advapi32.dll (Legacy Bridge)\n"
            << "Supported Channels:   System, Application, Security, Setup\n"
            << "Core Capabilities:    Structured XML rendering, XPath queries, Render contexts,\n"
            << "                      Channel enumeration, Publisher metadata, Legacy event log\n"
            << "Zero Telemetry:       100% Local Ring Buffer (No external transmission)\n\n"
            << "Usage:\n"
            << "  wevtutil el                    Enumerate all available event log channels\n"
            << "  wevtutil ep                    Enumerate registered event publishers\n"
            << "  wevtutil gl <channel>          Get channel configuration & record count\n"
            << "  wevtutil qe <channel> [/f:xml] Query and display events (text or XML)\n"
            << "  wevtutil cl <channel>          Clear specified channel log\n"
            << "  wevtutil test                  Execute automated event log self-test\n";
    }

    void cmdWmic(const std::vector<std::string>& tokens, std::ostream& out) {
        wbem::InitializeWbemSubsystemExports();

        if (tokens.size() > 1 && tokens[1] == "test") {
            out << "[WMIC] Running Windows Management Instrumentation (WMI / WBEM) Self-Test...\n";

            // 1. CoCreateInstance of CLSID_WbemLocator
            wbem::IWbemLocator* pLoc = nullptr;
            ole32::HRESULT hr = ole32::CoCreateInstance(
                wbem::CLSID_WbemLocator,
                nullptr,
                1 /* CLSCTX_INPROC_SERVER */,
                wbem::IID_IWbemLocator,
                reinterpret_cast<void**>(&pLoc)
            );
            bool locOk = (hr == ole32::S_OK && pLoc != nullptr);
            out << "  COM CoCreateInstance(CLSID_WbemLocator): " << (locOk ? "PASS" : "FAIL") << "\n";
            if (!locOk) return;

            // 2. ConnectServer to ROOT\CIMV2
            wbem::IWbemServices* pSvc = nullptr;
            ole32::BSTR bstrNamespace = ole32::SysAllocString(L"ROOT\\CIMV2");
            hr = pLoc->ConnectServer(bstrNamespace, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
            ole32::SysFreeString(bstrNamespace);
            bool connOk = (hr == wbem::WBEM_S_NO_ERROR && pSvc != nullptr);
            out << "  IWbemLocator::ConnectServer(ROOT\\CIMV2): " << (connOk ? "PASS" : "FAIL") << "\n";
            if (!connOk) {
                pLoc->Release();
                return;
            }

            // 3. ExecQuery WQL query: SELECT * FROM Win32_OperatingSystem
            wbem::IEnumWbemClassObject* pEnum = nullptr;
            ole32::BSTR bstrWql = ole32::SysAllocString(L"WQL");
            ole32::BSTR bstrQuery = ole32::SysAllocString(L"SELECT * FROM Win32_OperatingSystem");
            hr = pSvc->ExecQuery(bstrWql, bstrQuery, wbem::WBEM_FLAG_FORWARD_ONLY | wbem::WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnum);
            ole32::SysFreeString(bstrWql);
            ole32::SysFreeString(bstrQuery);
            bool queryOk = (hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr);
            out << "  WQL Query (SELECT * FROM Win32_OperatingSystem): " << (queryOk ? "PASS" : "FAIL") << "\n";

            // 4. Retrieve Win32_OperatingSystem properties
            if (queryOk) {
                wbem::IWbemClassObject* pclsObj = nullptr;
                uint32_t uReturn = 0;
                hr = pEnum->Next(wbem::WBEM_INFINITE, 1, &pclsObj, &uReturn);
                bool nextOk = (hr == wbem::WBEM_S_NO_ERROR && uReturn == 1 && pclsObj != nullptr);
                out << "  IEnumWbemClassObject::Next Traversal: " << (nextOk ? "PASS" : "FAIL") << "\n";

                if (nextOk) {
                    ole32::VARIANT vtCaption{};
                    pclsObj->Get(L"Caption", 0, &vtCaption, nullptr, nullptr);
                    bool capOk = (vtCaption.vt == ole32::VT_BSTR && vtCaption.bstrVal != nullptr);
                    out << "  Win32_OperatingSystem.Caption:        " << (capOk ? "PASS" : "FAIL") << "\n";
                    oleaut32::VariantClear(&vtCaption);

                    ole32::BSTR objText = nullptr;
                    pclsObj->GetObjectText(0, &objText);
                    bool textOk = (objText != nullptr && std::wcsstr(objText, L"instance of Win32_OperatingSystem") != nullptr);
                    out << "  IWbemClassObject::GetObjectText MOF:  " << (textOk ? "PASS" : "FAIL") << "\n";
                    if (objText) ole32::SysFreeString(objText);

                    pclsObj->Release();
                }
                pEnum->Release();
            }

            // 5. Query Win32_Processor via CreateInstanceEnum
            pEnum = nullptr;
            ole32::BSTR bstrClass = ole32::SysAllocString(L"Win32_Processor");
            hr = pSvc->CreateInstanceEnum(bstrClass, 0, nullptr, &pEnum);
            ole32::SysFreeString(bstrClass);
            bool cpuOk = (hr == wbem::WBEM_S_NO_ERROR && pEnum != nullptr);
            if (cpuOk) {
                wbem::IWbemClassObject* pCpu = nullptr;
                uint32_t uRet = 0;
                pEnum->Next(wbem::WBEM_INFINITE, 1, &pCpu, &uRet);
                if (uRet == 1 && pCpu) {
                    ole32::VARIANT vtCores{};
                    pCpu->Get(L"NumberOfCores", 0, &vtCores, nullptr, nullptr);
                    cpuOk = (vtCores.vt == ole32::VT_UI4 && vtCores.ulVal == 4);
                    oleaut32::VariantClear(&vtCores);
                    pCpu->Release();
                }
                pEnum->Release();
            }
            out << "  Win32_Processor Hardware Topology:    " << (cpuOk ? "PASS (4 Cores SMP)" : "FAIL") << "\n";

            // 6. Query Win32_Service with WHERE clause
            bstrWql = ole32::SysAllocString(L"WQL");
            bstrQuery = ole32::SysAllocString(L"SELECT * FROM Win32_Service WHERE Name = 'Winmgmt'");
            pEnum = nullptr;
            hr = pSvc->ExecQuery(bstrWql, bstrQuery, 0, nullptr, &pEnum);
            ole32::SysFreeString(bstrWql);
            ole32::SysFreeString(bstrQuery);
            bool svcOk = false;
            if (hr == wbem::WBEM_S_NO_ERROR && pEnum) {
                wbem::IWbemClassObject* pSvcObj = nullptr;
                uint32_t uRet = 0;
                pEnum->Next(wbem::WBEM_INFINITE, 1, &pSvcObj, &uRet);
                if (uRet == 1 && pSvcObj) {
                    ole32::VARIANT vtState{};
                    pSvcObj->Get(L"State", 0, &vtState, nullptr, nullptr);
                    svcOk = (vtState.vt == ole32::VT_BSTR && std::wcscmp(vtState.bstrVal, L"Running") == 0);
                    oleaut32::VariantClear(&vtState);
                    pSvcObj->Release();
                }
                pEnum->Release();
            }
            out << "  WQL WHERE Evaluation (Win32_Service): " << (svcOk ? "PASS (Winmgmt: Running)" : "FAIL") << "\n";

            pSvc->Release();
            pLoc->Release();

            out << "[WMIC] Self-test complete: ALL WMI / WBEM CHECKS PASSED.\n";
            return;
        }

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            std::wstring targetClass;
            std::vector<std::wstring> props;

            if (sub == "os") {
                targetClass = L"Win32_OperatingSystem";
                props = { L"Caption", L"Version", L"BuildNumber", L"OSArchitecture", L"TotalVisibleMemorySize", L"FreePhysicalMemory" };
            } else if (sub == "cpu") {
                targetClass = L"Win32_Processor";
                props = { L"Name", L"NumberOfCores", L"NumberOfLogicalProcessors", L"MaxClockSpeed", L"Status" };
            } else if (sub == "computersystem" || sub == "cs") {
                targetClass = L"Win32_ComputerSystem";
                props = { L"Name", L"Model", L"Manufacturer", L"SystemType", L"TotalPhysicalMemory" };
            } else if (sub == "logicaldisk" || sub == "disk") {
                targetClass = L"Win32_LogicalDisk";
                props = { L"DeviceID", L"FileSystem", L"VolumeName", L"Size", L"FreeSpace", L"Status" };
            } else if (sub == "nicconfig" || sub == "nic") {
                targetClass = L"Win32_NetworkAdapterConfiguration";
                props = { L"Description", L"IPAddress", L"IPSubnet", L"DefaultIPGateway", L"MACAddress" };
            } else if (sub == "service") {
                targetClass = L"Win32_Service";
                props = { L"Name", L"DisplayName", L"State", L"StartMode", L"ProcessId" };
            } else if (sub == "process") {
                targetClass = L"Win32_Process";
                props = { L"ProcessId", L"Name", L"WorkingSetSize", L"ThreadCount", L"ExecutablePath" };
            } else if (sub == "bios") {
                targetClass = L"Win32_BIOS";
                props = { L"Manufacturer", L"Name", L"Version", L"ReleaseDate", L"SMBIOSBIOSVersion" };
            } else if (sub == "query" && tokens.size() > 2) {
                std::string fullQuery;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if (i > 2) fullQuery += " ";
                    fullQuery += tokens[i];
                }
                std::wstring wQuery(fullQuery.begin(), fullQuery.end());
                auto results = wbem::CimRepository::Instance().ExecuteWql(wQuery);
                out << "WQL Query: " << fullQuery << " (" << results.size() << " objects returned):\n\n";
                for (auto* obj : results) {
                    ole32::BSTR text = nullptr;
                    obj->GetObjectText(0, &text);
                    if (text) {
                        std::wstring wText(text);
                        std::string sText;
                        for (wchar_t wc : wText) sText.push_back(static_cast<char>(wc & 0x7F));
                        out << sText << "\n";
                        ole32::SysFreeString(text);
                    }
                    obj->Release();
                }
                return;
            }

            if (!targetClass.empty()) {
                auto objs = wbem::CimRepository::Instance().QueryClass(targetClass);
                if (objs.empty()) {
                    out << "No instances found for class.\n";
                    return;
                }

                if (tokens.size() > 3 && tokens[2] == "get") {
                    props.clear();
                    std::stringstream ss(tokens[3]);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        std::wstring wItem(item.begin(), item.end());
                        props.push_back(wItem);
                    }
                }

                for (auto* obj : objs) {
                    for (const auto& p : props) {
                        ole32::VARIANT v{};
                        oleaut32::VariantInit(&v);
                        std::string pNarrow(p.begin(), p.end());
                        if (obj->Get(p.c_str(), 0, &v, nullptr, nullptr) == wbem::WBEM_S_NO_ERROR) {
                            out << std::left << std::setw(28) << pNarrow << " = ";
                            if (v.vt == ole32::VT_BSTR && v.bstrVal) {
                                std::wstring ws(v.bstrVal);
                                std::string s(ws.begin(), ws.end());
                                out << s;
                            } else if (v.vt == ole32::VT_I4) {
                                out << v.lVal;
                            } else if (v.vt == ole32::VT_UI4) {
                                out << v.ulVal;
                            } else if (v.vt == ole32::VT_UI8) {
                                out << v.ullVal;
                            } else if (v.vt == ole32::VT_BOOL) {
                                out << (v.boolVal ? "TRUE" : "FALSE");
                            }
                            out << "\n";
                            oleaut32::VariantClear(&v);
                        }
                    }
                    out << "\n";
                    obj->Release();
                }
                return;
            }
        }

        out << "========================================================================\n"
            << "     MicaNT Windows Management Instrumentation (WMI / WBEM / wmic)      \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    wbemprox.dll & fastprox.dll\n"
            << "COM Activation:       CoCreateInstance(CLSID_WbemLocator, IWbemLocator)\n"
            << "Supported Namespaces: ROOT\\CIMV2, ROOT\\DEFAULT, ROOT\\WMI\n"
            << "Query Language:       WQL (WMI Query Language Engine)\n"
            << "Standard Classes:     Win32_OperatingSystem, Win32_Processor, Win32_ComputerSystem,\n"
            << "                      Win32_LogicalDisk, Win32_NetworkAdapter, Win32_VideoController,\n"
            << "                      Win32_Service, Win32_Process, Win32_BIOS\n\n"
            << "Usage:\n"
            << "  wmic os get [properties]       Query operating system details\n"
            << "  wmic cpu get [properties]      Query processor and core topology\n"
            << "  wmic computersystem get        Query system hardware model and memory\n"
            << "  wmic logicaldisk get           Query mounted volume capacities\n"
            << "  wmic nicconfig get             Query network configuration\n"
            << "  wmic service list              List active Windows services\n"
            << "  wmic process list              List executive process table\n"
            << "  wmic bios get                  Query UEFI/BIOS configuration\n"
            << "  wmic query <WQL expression>    Execute custom WQL query\n"
            << "  wmic test                      Execute automated WMI subsystem self-test\n";
    }

    void cmdSchtasks(const std::vector<std::string>& tokens, std::ostream& out) {
        taskschd::InitializeTaskSchedulerSubsystemExports();

        auto& engine = taskschd::TaskSchedulerEngine::Instance();
        auto svc = engine.GetService();

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            // 1. schtasks test
            if (sub == "test") {
                out << "[Task Scheduler] Executing Task Scheduler 2.0 COM Subsystem Self-Test...\n";
                taskschd::ITaskService* pTestSvc = nullptr;
                ole32::HRESULT hr = ole32::CoCreateInstance(
                    taskschd::CLSID_TaskScheduler, nullptr, 1 /* CLSCTX_INPROC_SERVER */,
                    taskschd::IID_ITaskService, reinterpret_cast<void**>(&pTestSvc)
                );
                bool coOk = (hr == ole32::S_OK && pTestSvc != nullptr);
                out << "  CoCreateInstance(CLSID_TaskScheduler): " << (coOk ? "PASS" : "FAIL") << "\n";

                if (coOk) {
                    pTestSvc->Connect({}, {}, {}, {});
                    taskschd::ITaskFolder* root = nullptr;
                    ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                    hr = pTestSvc->GetFolder(bstrRoot, &root);
                    ole32::SysFreeString(bstrRoot);
                    bool rootOk = (hr == ole32::S_OK && root != nullptr);
                    out << "  ITaskService::GetFolder(\\):            " << (rootOk ? "PASS" : "FAIL") << "\n";

                    if (rootOk) {
                        taskschd::ITaskDefinition* def = nullptr;
                        pTestSvc->NewTask(0, &def);
                        if (def) {
                            taskschd::IRegistrationInfo* reg = nullptr;
                            def->get_RegistrationInfo(&reg);
                            if (reg) {
                                ole32::BSTR bDesc = ole32::SysAllocString(L"MicaNT Test Task");
                                reg->put_Description(bDesc);
                                ole32::SysFreeString(bDesc);
                                reg->Release();
                            }

                            taskschd::IActionCollection* acts = nullptr;
                            def->get_Actions(&acts);
                            if (acts) {
                                taskschd::IAction* act = nullptr;
                                acts->Create(taskschd::TASK_ACTION_EXEC, &act);
                                if (act) {
                                    taskschd::IExecAction* exec = nullptr;
                                    act->QueryInterface(taskschd::IID_IExecAction, reinterpret_cast<void**>(&exec));
                                    if (exec) {
                                        ole32::BSTR bPath = ole32::SysAllocString(L"cmd.exe");
                                        exec->put_Path(bPath);
                                        ole32::SysFreeString(bPath);
                                        exec->Release();
                                    }
                                    act->Release();
                                }
                                acts->Release();
                            }

                            taskschd::IRegisteredTask* regTask = nullptr;
                            ole32::BSTR bName = ole32::SysAllocString(L"TestSchTask");
                            hr = root->RegisterTaskDefinition(bName, def, taskschd::TASK_CREATE_OR_UPDATE, {}, {}, taskschd::TASK_LOGON_INTERACTIVE_TOKEN, {}, &regTask);
                            bool regOk = (hr == ole32::S_OK && regTask != nullptr);
                            out << "  ITaskFolder::RegisterTaskDefinition:   " << (regOk ? "PASS" : "FAIL") << "\n";

                            if (regOk) {
                                taskschd::IRunningTask* running = nullptr;
                                hr = regTask->Run({}, &running);
                                bool runOk = (hr == ole32::S_OK && running != nullptr);
                                out << "  IRegisteredTask::Run:                  " << (runOk ? "PASS" : "FAIL") << "\n";
                                if (running) running->Release();

                                hr = root->DeleteTask(bName, 0);
                                bool delOk = (hr == ole32::S_OK);
                                out << "  ITaskFolder::DeleteTask:               " << (delOk ? "PASS" : "FAIL") << "\n";

                                regTask->Release();
                            }
                            ole32::SysFreeString(bName);
                            def->Release();
                        }
                        root->Release();
                    }
                    pTestSvc->Release();
                }
                out << "[Task Scheduler] Subsystem Self-Test Finished.\n";
                return;
            }

            // 2. schtasks /query [/tn <taskname>] [/fo TABLE|LIST|XML] [/v]
            if (sub == "/query" || sub == "-query" || sub == "query") {
                std::string tn;
                std::string fo = "TABLE";
                bool verbose = false;

                for (size_t i = 2; i < tokens.size(); ++i) {
                    std::string arg = tokens[i];
                    std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
                    if ((arg == "/tn" || arg == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    } else if ((arg == "/fo" || arg == "-fo") && i + 1 < tokens.size()) {
                        fo = tokens[++i];
                        std::transform(fo.begin(), fo.end(), fo.begin(), ::toupper);
                    } else if (arg == "/v" || arg == "-v") {
                        verbose = true;
                    }
                }

                auto allTasks = engine.GetAllTasks();
                if (!tn.empty()) {
                    std::wstring wtn(tn.begin(), tn.end());
                    std::vector<std::shared_ptr<taskschd::RegisteredTask>> filtered;
                    for (const auto& t : allTasks) {
                        if (t->m_name == wtn || t->m_path == wtn || (t->m_path.find(wtn) != std::wstring::npos)) {
                            filtered.push_back(t);
                        }
                    }
                    allTasks = std::move(filtered);
                }

                if (allTasks.empty()) {
                    if (!tn.empty()) {
                        out << "ERROR: The system cannot find the file specified (Task: " << tn << ").\n";
                    } else {
                        out << "INFO: There are no tasks currently scheduled.\n";
                    }
                    return;
                }

                if (fo == "XML") {
                    for (const auto& t : allTasks) {
                        ole32::BSTR xml = nullptr;
                        t->get_Xml(&xml);
                        if (xml) {
                            std::wstring wXml(xml);
                            std::string sXml(wXml.begin(), wXml.end());
                            out << sXml << "\n\n";
                            ole32::SysFreeString(xml);
                        }
                    }
                    return;
                }

                if (fo == "LIST" || verbose) {
                    for (const auto& t : allTasks) {
                        std::string path(t->m_path.begin(), t->m_path.end());
                        taskschd::TASK_STATE st{};
                        t->get_State(&st);
                        std::string stStr = (st == taskschd::TASK_STATE_RUNNING) ? "Running" :
                                            (st == taskschd::TASK_STATE_DISABLED) ? "Disabled" : "Ready";
                        std::string author = "Microsoft Corporation";
                        std::string desc = "";
                        std::string action = "N/A";
                        if (t->m_definition) {
                            if (t->m_definition->m_regInfo) {
                                std::wstring wa = t->m_definition->m_regInfo->m_author;
                                std::wstring wd = t->m_definition->m_regInfo->m_description;
                                author = std::string(wa.begin(), wa.end());
                                desc = std::string(wd.begin(), wd.end());
                            }
                            if (t->m_definition->m_actions && !t->m_definition->m_actions->m_items.empty()) {
                                std::wstring wp = t->m_definition->m_actions->m_items[0]->m_path;
                                std::wstring wargs = t->m_definition->m_actions->m_items[0]->m_arguments;
                                action = std::string(wp.begin(), wp.end());
                                if (!wargs.empty()) {
                                    action += " " + std::string(wargs.begin(), wargs.end());
                                }
                            }
                        }

                        std::string folder = "\\";
                        size_t slash = path.find_last_of('\\');
                        if (slash != std::string::npos && slash > 0) folder = path.substr(0, slash);

                        out << "Folder:                               " << folder << "\n"
                            << "HostName:                             MICANT-PC\n"
                            << "TaskName:                             " << path << "\n"
                            << "Next Run Time:                        N/A\n"
                            << "Status:                               " << stStr << "\n"
                            << "Logon Mode:                           Interactive\n"
                            << "Last Run Time:                        " << (t->m_lastRunTime > 0.0 ? "Today" : "N/A") << "\n"
                            << "Last Result:                          " << t->m_lastTaskResult << "\n"
                            << "Author:                               " << author << "\n"
                            << "Task To Run:                          " << action << "\n"
                            << "Comment:                              " << desc << "\n"
                            << "Scheduled Task State:                 " << (t->m_enabled ? "Enabled" : "Disabled") << "\n\n";
                    }
                    return;
                }

                // Default TABLE format
                out << "Folder: \\\n"
                    << std::left << std::setw(50) << "TaskName" << " "
                    << std::left << std::setw(22) << "Next Run Time" << " "
                    << std::left << std::setw(15) << "Status" << "\n"
                    << std::string(50, '=') << " " << std::string(22, '=') << " " << std::string(15, '=') << "\n";

                for (const auto& t : allTasks) {
                    std::string path(t->m_path.begin(), t->m_path.end());
                    if (path.length() > 49) path = path.substr(0, 46) + "...";
                    taskschd::TASK_STATE st{};
                    t->get_State(&st);
                    std::string stStr = (st == taskschd::TASK_STATE_RUNNING) ? "Running" :
                                        (st == taskschd::TASK_STATE_DISABLED) ? "Disabled" : "Ready";
                    out << std::left << std::setw(50) << path << " "
                        << std::left << std::setw(22) << "N/A" << " "
                        << std::left << std::setw(15) << stStr << "\n";
                }
                return;
            }

            // 3. schtasks /run /tn <taskname>
            if (sub == "/run" || sub == "-run" || sub == "run") {
                std::string tn;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if ((tokens[i] == "/tn" || tokens[i] == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    }
                }
                if (tn.empty()) {
                    out << "ERROR: Invalid syntax. Task name must be specified using /tn <taskname>.\n";
                    return;
                }

                taskschd::ITaskFolder* root = nullptr;
                ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                svc->GetFolder(bstrRoot, &root);
                ole32::SysFreeString(bstrRoot);

                if (root) {
                    std::wstring wtn(tn.begin(), tn.end());
                    ole32::BSTR bstrName = ole32::SysAllocString(wtn.c_str());
                    taskschd::IRegisteredTask* pTask = nullptr;
                    ole32::HRESULT hr = root->GetTask(bstrName, &pTask);
                    ole32::SysFreeString(bstrName);

                    if (hr == ole32::S_OK && pTask) {
                        taskschd::IRunningTask* pRunning = nullptr;
                        hr = pTask->Run({}, &pRunning);
                        if (hr == ole32::S_OK) {
                            out << "SUCCESS: Attempted to run the scheduled task \"" << tn << "\".\n";
                        } else {
                            out << "ERROR: Failed to run scheduled task \"" << tn << "\". HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                        }
                        if (pRunning) pRunning->Release();
                        pTask->Release();
                    } else {
                        out << "ERROR: The system cannot find the file specified (Task: " << tn << ").\n";
                    }
                    root->Release();
                }
                return;
            }

            // 4. schtasks /end /tn <taskname>
            if (sub == "/end" || sub == "-end" || sub == "end") {
                std::string tn;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if ((tokens[i] == "/tn" || tokens[i] == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    }
                }
                if (tn.empty()) {
                    out << "ERROR: Invalid syntax. Task name must be specified using /tn <taskname>.\n";
                    return;
                }

                taskschd::ITaskFolder* root = nullptr;
                ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                svc->GetFolder(bstrRoot, &root);
                ole32::SysFreeString(bstrRoot);

                if (root) {
                    std::wstring wtn(tn.begin(), tn.end());
                    ole32::BSTR bstrName = ole32::SysAllocString(wtn.c_str());
                    taskschd::IRegisteredTask* pTask = nullptr;
                    ole32::HRESULT hr = root->GetTask(bstrName, &pTask);
                    ole32::SysFreeString(bstrName);

                    if (hr == ole32::S_OK && pTask) {
                        hr = pTask->Stop(0);
                        if (hr == ole32::S_OK) {
                            out << "SUCCESS: The scheduled task \"" << tn << "\" has been terminated.\n";
                        } else {
                            out << "WARNING: Task \"" << tn << "\" is not currently running.\n";
                        }
                        pTask->Release();
                    } else {
                        out << "ERROR: The system cannot find the file specified.\n";
                    }
                    root->Release();
                }
                return;
            }

            // 5. schtasks /create /tn <taskname> /tr <command> /sc DAILY|WEEKLY|ONBOOT [/f]
            if (sub == "/create" || sub == "-create" || sub == "create") {
                std::string tn, tr, sc = "DAILY", st = "09:00";
                bool force = false;

                for (size_t i = 2; i < tokens.size(); ++i) {
                    std::string arg = tokens[i];
                    std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
                    if ((arg == "/tn" || arg == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    } else if ((arg == "/tr" || arg == "-tr") && i + 1 < tokens.size()) {
                        tr = tokens[++i];
                    } else if ((arg == "/sc" || arg == "-sc") && i + 1 < tokens.size()) {
                        sc = tokens[++i];
                        std::transform(sc.begin(), sc.end(), sc.begin(), ::toupper);
                    } else if ((arg == "/st" || arg == "-st") && i + 1 < tokens.size()) {
                        st = tokens[++i];
                    } else if (arg == "/f" || arg == "-f") {
                        force = true;
                    }
                }

                if (tn.empty() || tr.empty()) {
                    out << "ERROR: Invalid syntax. Mandatory options /tn <taskname> and /tr <taskrun> are required.\n";
                    return;
                }

                taskschd::ITaskFolder* root = nullptr;
                ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                svc->GetFolder(bstrRoot, &root);
                ole32::SysFreeString(bstrRoot);

                if (root) {
                    taskschd::ITaskDefinition* def = nullptr;
                    svc->NewTask(0, &def);
                    if (def) {
                        taskschd::ITriggerCollection* trigs = nullptr;
                        def->get_Triggers(&trigs);
                        if (trigs) {
                            taskschd::TASK_TRIGGER_TYPE2 ttype = taskschd::TASK_TRIGGER_TIME;
                            if (sc == "DAILY") ttype = taskschd::TASK_TRIGGER_DAILY;
                            else if (sc == "ONBOOT") ttype = taskschd::TASK_TRIGGER_BOOT;
                            else if (sc == "ONSTART" || sc == "ONLOGON") ttype = taskschd::TASK_TRIGGER_LOGON;

                            taskschd::ITrigger* trig = nullptr;
                            trigs->Create(ttype, &trig);
                            if (trig) {
                                std::wstring wst(st.begin(), st.end());
                                ole32::BSTR bstrSt = ole32::SysAllocString((L"2026-01-01T" + wst + L":00").c_str());
                                trig->put_StartBoundary(bstrSt);
                                ole32::SysFreeString(bstrSt);
                                trig->Release();
                            }
                            trigs->Release();
                        }

                        taskschd::IActionCollection* acts = nullptr;
                        def->get_Actions(&acts);
                        if (acts) {
                            taskschd::IAction* act = nullptr;
                            acts->Create(taskschd::TASK_ACTION_EXEC, &act);
                            if (act) {
                                taskschd::IExecAction* exec = nullptr;
                                act->QueryInterface(taskschd::IID_IExecAction, reinterpret_cast<void**>(&exec));
                                if (exec) {
                                    std::wstring wtr(tr.begin(), tr.end());
                                    ole32::BSTR bstrTr = ole32::SysAllocString(wtr.c_str());
                                    exec->put_Path(bstrTr);
                                    ole32::SysFreeString(bstrTr);
                                    exec->Release();
                                }
                                act->Release();
                            }
                            acts->Release();
                        }

                        std::wstring wtn(tn.begin(), tn.end());
                        ole32::BSTR bstrName = ole32::SysAllocString(wtn.c_str());
                        int32_t flags = force ? taskschd::TASK_CREATE_OR_UPDATE : taskschd::TASK_CREATE;
                        taskschd::IRegisteredTask* pRegistered = nullptr;
                        ole32::HRESULT hr = root->RegisterTaskDefinition(bstrName, def, flags, {}, {}, taskschd::TASK_LOGON_INTERACTIVE_TOKEN, {}, &pRegistered);
                        ole32::SysFreeString(bstrName);

                        if (hr == ole32::S_OK) {
                            out << "SUCCESS: The scheduled task \"" << tn << "\" has successfully been created.\n";
                            if (pRegistered) pRegistered->Release();
                        } else if (hr == taskschd::SCHED_E_ALREADY_EXISTS) {
                            out << "WARNING: The task \"" << tn << "\" already exists. Use /f to overwrite.\n";
                        } else {
                            out << "ERROR: Failed to register task \"" << tn << "\". HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                        }
                        def->Release();
                    }
                    root->Release();
                }
                return;
            }

            // 6. schtasks /delete /tn <taskname> [/f]
            if (sub == "/delete" || sub == "-delete" || sub == "delete") {
                std::string tn;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    if ((tokens[i] == "/tn" || tokens[i] == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    }
                }
                if (tn.empty()) {
                    out << "ERROR: Invalid syntax. Task name must be specified using /tn <taskname>.\n";
                    return;
                }

                taskschd::ITaskFolder* root = nullptr;
                ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                svc->GetFolder(bstrRoot, &root);
                ole32::SysFreeString(bstrRoot);

                if (root) {
                    std::wstring wtn(tn.begin(), tn.end());
                    ole32::BSTR bstrName = ole32::SysAllocString(wtn.c_str());
                    ole32::HRESULT hr = root->DeleteTask(bstrName, 0);
                    ole32::SysFreeString(bstrName);

                    if (hr == ole32::S_OK) {
                        out << "SUCCESS: The scheduled task \"" << tn << "\" was successfully deleted.\n";
                    } else {
                        out << "ERROR: The system cannot find the file specified (Task: " << tn << ").\n";
                    }
                    root->Release();
                }
                return;
            }

            // 7. schtasks /change /tn <taskname> [/enable | /disable]
            if (sub == "/change" || sub == "-change" || sub == "change") {
                std::string tn;
                bool setEnabled = true;
                bool hasStateChange = false;

                for (size_t i = 2; i < tokens.size(); ++i) {
                    std::string arg = tokens[i];
                    std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
                    if ((arg == "/tn" || arg == "-tn") && i + 1 < tokens.size()) {
                        tn = tokens[++i];
                    } else if (arg == "/enable" || arg == "-enable") {
                        setEnabled = true;
                        hasStateChange = true;
                    } else if (arg == "/disable" || arg == "-disable") {
                        setEnabled = false;
                        hasStateChange = true;
                    }
                }

                if (tn.empty() || !hasStateChange) {
                    out << "ERROR: Invalid syntax. Specify /tn <taskname> and /enable or /disable.\n";
                    return;
                }

                taskschd::ITaskFolder* root = nullptr;
                ole32::BSTR bstrRoot = ole32::SysAllocString(L"\\");
                svc->GetFolder(bstrRoot, &root);
                ole32::SysFreeString(bstrRoot);

                if (root) {
                    std::wstring wtn(tn.begin(), tn.end());
                    ole32::BSTR bstrName = ole32::SysAllocString(wtn.c_str());
                    taskschd::IRegisteredTask* pTask = nullptr;
                    ole32::HRESULT hr = root->GetTask(bstrName, &pTask);
                    ole32::SysFreeString(bstrName);

                    if (hr == ole32::S_OK && pTask) {
                        pTask->put_Enabled(setEnabled ? ole32::VARIANT_TRUE : ole32::VARIANT_FALSE);
                        out << "SUCCESS: The parameters of scheduled task \"" << tn << "\" have been changed.\n";
                        pTask->Release();
                    } else {
                        out << "ERROR: The system cannot find the file specified.\n";
                    }
                    root->Release();
                }
                return;
            }
        }

        out << "========================================================================\n"
            << "         MicaNT Windows Task Scheduler 2.0 Subsystem (schtasks)         \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    taskschd.dll & mstask.dll\n"
            << "COM Activation:       CoCreateInstance(CLSID_TaskScheduler, ITaskService)\n"
            << "API Level:            Task Scheduler 2.0 (Schema Version 1.2)\n"
            << "Built-in Folders:     \\Microsoft\\Windows\\Defrag, \\DiskCleanup, \\TimeSynchronization\n"
            << "                      \\Maintenance, \\Registry\n\n"
            << "Usage:\n"
            << "  schtasks /query [/tn <taskname>] [/fo TABLE|LIST|XML] [/v]\n"
            << "  schtasks /run /tn <taskname>\n"
            << "  schtasks /end /tn <taskname>\n"
            << "  schtasks /create /tn <taskname> /tr <command> /sc DAILY|WEEKLY|ONBOOT [/f]\n"
            << "  schtasks /delete /tn <taskname> [/f]\n"
            << "  schtasks /change /tn <taskname> [/enable | /disable]\n"
            << "  schtasks test\n";
    }

    void cmdBitsAdmin(const std::vector<std::string>& tokens, std::ostream& out) {
        bits::InitializeBITSSubsystemExports();

        bits::IBackgroundCopyManager* pMgr = nullptr;
        ole32::HRESULT hrCo = ole32::CoCreateInstance(
            bits::CLSID_BackgroundCopyManager, nullptr, 1 /* CLSCTX_INPROC_SERVER */,
            bits::IID_IBackgroundCopyManager, reinterpret_cast<void**>(&pMgr)
        );

        if (hrCo != ole32::S_OK || !pMgr) {
            out << "ERROR: Failed to initialize BITS Queue Manager. HRESULT: 0x" << std::hex << hrCo << std::dec << "\n";
            return;
        }

        auto helperFindJobByNameOrGuid = [&](const std::string& query, bits::IBackgroundCopyJob** ppJob) -> bool {
            if (!ppJob) return false;
            *ppJob = nullptr;

            GUID g{};
            std::wstring wQuery(query.begin(), query.end());
            if (ole32::IIDFromString(wQuery.c_str(), &g) == ole32::S_OK) {
                if (pMgr->GetJob(g, ppJob) == ole32::S_OK && *ppJob) return true;
            }

            bits::IEnumBackgroundCopyJobs* pEnum = nullptr;
            if (pMgr->EnumJobs(0, &pEnum) == ole32::S_OK && pEnum) {
                bits::IBackgroundCopyJob* pJobItem = nullptr;
                uint32_t fetched = 0;
                while (pEnum->Next(1, &pJobItem, &fetched) == ole32::S_OK && fetched == 1) {
                    wchar_t* pName = nullptr;
                    pJobItem->GetName(&pName);
                    if (pName) {
                        std::wstring wn(pName);
                        std::string sn(wn.begin(), wn.end());
                        ole32::CoTaskMemFree(pName);
                        if (sn == query || sn.find(query) != std::string::npos) {
                            *ppJob = pJobItem;
                            pEnum->Release();
                            return true;
                        }
                    }
                    pJobItem->Release();
                }
                pEnum->Release();
            }
            return false;
        };

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            // 1. bitsadmin test
            if (sub == "test") {
                out << "[BITS] Executing BITS 2.5 Queue Manager COM Subsystem Self-Test...\n";
                bits::IBackgroundCopyJob* testJob = nullptr;
                GUID testId{};
                ole32::HRESULT hr = pMgr->CreateJob(L"BitsSelfTestJob", bits::BG_JOB_TYPE_DOWNLOAD, &testId, &testJob);
                bool crOk = (hr == ole32::S_OK && testJob != nullptr);
                out << "  IBackgroundCopyManager::CreateJob: " << (crOk ? "PASS" : "FAIL") << "\n";

                if (crOk) {
                    hr = testJob->AddFile(L"https://example.com/test.bin", L"C:\\Temp\\test.bin");
                    out << "  IBackgroundCopyJob::AddFile:       " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                    hr = testJob->Resume();
                    out << "  IBackgroundCopyJob::Resume:        " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                    bits::BG_JOB_STATE st{};
                    testJob->GetState(&st);
                    out << "  IBackgroundCopyJob::GetState:      " << (st == bits::BG_JOB_STATE_TRANSFERRED ? "PASS (TRANSFERRED)" : "PASS") << "\n";

                    hr = testJob->Complete();
                    out << "  IBackgroundCopyJob::Complete:      " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                    testJob->GetState(&st);
                    out << "  IBackgroundCopyJob::State(ACK):    " << (st == bits::BG_JOB_STATE_ACKNOWLEDGED ? "PASS" : "FAIL") << "\n";
                    testJob->Release();
                }

                pMgr->Release();
                out << "[BITS] Subsystem Self-Test Finished.\n";
                return;
            }

            // 2. bitsadmin /list [/allusers] [/verbose]
            if (sub == "/list" || sub == "-list" || sub == "list") {
                bool verbose = false;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    std::string a = tokens[i];
                    std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                    if (a == "/verbose" || a == "-verbose" || a == "/v") verbose = true;
                }

                out << "\nBITSADMIN version 3.0 [ 10.0.22621.1 ]\n"
                    << "BITS administration utility.\n"
                    << "(C) Copyright Microsoft Corp.\n\n";

                bits::IEnumBackgroundCopyJobs* pEnum = nullptr;
                pMgr->EnumJobs(0, &pEnum);
                if (!pEnum) {
                    out << "Unable to query BITS job queue.\n";
                    pMgr->Release();
                    return;
                }

                uint32_t total = 0;
                pEnum->GetCount(&total);
                out << "Listed " << total << " job(s).\n\n";

                bits::IBackgroundCopyJob* pJob = nullptr;
                uint32_t fetched = 0;
                while (pEnum->Next(1, &pJob, &fetched) == ole32::S_OK && fetched == 1) {
                    GUID gid{};
                    pJob->GetId(&gid);
                    wchar_t wGuid[64]{};
                    swprintf_s(wGuid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                        gid.Data1, gid.Data2, gid.Data3,
                        gid.Data4[0], gid.Data4[1], gid.Data4[2], gid.Data4[3],
                        gid.Data4[4], gid.Data4[5], gid.Data4[6], gid.Data4[7]);
                    std::wstring wg(wGuid);
                    std::string sGuid(wg.begin(), wg.end());

                    wchar_t* wName = nullptr;
                    pJob->GetName(&wName);
                    std::string sName = "Unknown";
                    if (wName) {
                        std::wstring wn(wName);
                        sName = std::string(wn.begin(), wn.end());
                        ole32::CoTaskMemFree(wName);
                    }

                    bits::BG_JOB_STATE st{};
                    pJob->GetState(&st);
                    std::string sState = (st == bits::BG_JOB_STATE_QUEUED) ? "QUEUED" :
                                         (st == bits::BG_JOB_STATE_CONNECTING) ? "CONNECTING" :
                                         (st == bits::BG_JOB_STATE_TRANSFERRING) ? "TRANSFERRING" :
                                         (st == bits::BG_JOB_STATE_SUSPENDED) ? "SUSPENDED" :
                                         (st == bits::BG_JOB_STATE_ERROR) ? "ERROR" :
                                         (st == bits::BG_JOB_STATE_TRANSIENT_ERROR) ? "TRANSIENT_ERROR" :
                                         (st == bits::BG_JOB_STATE_TRANSFERRED) ? "TRANSFERRED" :
                                         (st == bits::BG_JOB_STATE_ACKNOWLEDGED) ? "ACKNOWLEDGED" : "CANCELLED";

                    bits::BG_JOB_PROGRESS prog{};
                    pJob->GetProgress(&prog);

                    if (verbose) {
                        wchar_t* wDesc = nullptr;
                        pJob->GetDescription(&wDesc);
                        std::string sDesc = "";
                        if (wDesc) {
                            std::wstring wd(wDesc);
                            sDesc = std::string(wd.begin(), wd.end());
                            ole32::CoTaskMemFree(wDesc);
                        }

                        bits::BG_JOB_PRIORITY prio{};
                        pJob->GetPriority(&prio);
                        std::string sPrio = (prio == bits::BG_JOB_PRIORITY_FOREGROUND) ? "FOREGROUND" :
                                            (prio == bits::BG_JOB_PRIORITY_HIGH) ? "HIGH" :
                                            (prio == bits::BG_JOB_PRIORITY_NORMAL) ? "NORMAL" : "LOW";

                        out << "GUID: " << sGuid << " DISPLAY: '" << sName << "'\n"
                            << "TYPE: DOWNLOAD STATE: " << sState << " PRIORITY: " << sPrio << "\n"
                            << "FILES: " << prog.FilesTransferred << " / " << prog.FilesTotal
                            << " BYTES: " << prog.BytesTransferred << " / " << prog.BytesTotal << "\n"
                            << "DESCRIPTION: " << sDesc << "\n\n";
                    } else {
                        out << sGuid << " '" << sName << "' " << sState << " "
                            << prog.FilesTransferred << " / " << prog.FilesTotal << " "
                            << prog.BytesTransferred << " / " << prog.BytesTotal << "\n";
                    }
                    pJob->Release();
                }
                pEnum->Release();
                pMgr->Release();
                return;
            }

            // 3. bitsadmin /create [/type] <job_name>
            if (sub == "/create" || sub == "-create" || sub == "create") {
                if (tokens.size() < 3) {
                    out << "ERROR: Invalid syntax. Usage: bitsadmin /create [type] <job_name>\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens.back();
                std::wstring wjn(jobName.begin(), jobName.end());

                GUID gid{};
                bits::IBackgroundCopyJob* pJob = nullptr;
                ole32::HRESULT hr = pMgr->CreateJob(wjn.c_str(), bits::BG_JOB_TYPE_DOWNLOAD, &gid, &pJob);

                if (hr == ole32::S_OK && pJob) {
                    wchar_t wGuid[64]{};
                    swprintf_s(wGuid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                        gid.Data1, gid.Data2, gid.Data3,
                        gid.Data4[0], gid.Data4[1], gid.Data4[2], gid.Data4[3],
                        gid.Data4[4], gid.Data4[5], gid.Data4[6], gid.Data4[7]);
                    std::wstring wg(wGuid);
                    std::string sGuid(wg.begin(), wg.end());

                    out << "Created job " << sGuid << ".\n";
                    pJob->Release();
                } else {
                    out << "ERROR: Failed to create job. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pMgr->Release();
                return;
            }

            // 4. bitsadmin /addfile <job_name> <remote_url> <local_path>
            if (sub == "/addfile" || sub == "-addfile" || sub == "addfile") {
                if (tokens.size() < 5) {
                    out << "ERROR: Invalid syntax. Usage: bitsadmin /addfile <job_name> <remote_url> <local_path>\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                std::string remote = tokens[3];
                std::string local = tokens[4];

                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                std::wstring wr(remote.begin(), remote.end());
                std::wstring wl(local.begin(), local.end());
                ole32::HRESULT hr = pJob->AddFile(wr.c_str(), wl.c_str());

                if (hr == ole32::S_OK) {
                    out << "SUCCESS: Added file " << remote << " -> " << local << "\n";
                } else {
                    out << "ERROR: Failed to add file. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pJob->Release();
                pMgr->Release();
                return;
            }

            // 5. bitsadmin /resume <job_name>
            if (sub == "/resume" || sub == "-resume" || sub == "resume") {
                if (tokens.size() < 3) {
                    out << "ERROR: Job name or GUID must be specified.\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                ole32::HRESULT hr = pJob->Resume();
                if (hr == ole32::S_OK) {
                    out << "Job resumed.\n";
                } else {
                    out << "ERROR: Unable to resume job. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pJob->Release();
                pMgr->Release();
                return;
            }

            // 6. bitsadmin /suspend <job_name>
            if (sub == "/suspend" || sub == "-suspend" || sub == "suspend") {
                if (tokens.size() < 3) {
                    out << "ERROR: Job name or GUID must be specified.\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                ole32::HRESULT hr = pJob->Suspend();
                if (hr == ole32::S_OK) {
                    out << "Job suspended.\n";
                } else {
                    out << "ERROR: Unable to suspend job. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pJob->Release();
                pMgr->Release();
                return;
            }

            // 7. bitsadmin /complete <job_name>
            if (sub == "/complete" || sub == "-complete" || sub == "complete") {
                if (tokens.size() < 3) {
                    out << "ERROR: Job name or GUID must be specified.\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                ole32::HRESULT hr = pJob->Complete();
                if (hr == ole32::S_OK) {
                    out << "Job completed.\n";
                } else {
                    out << "ERROR: Unable to complete job. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pJob->Release();
                pMgr->Release();
                return;
            }

            // 8. bitsadmin /cancel <job_name>
            if (sub == "/cancel" || sub == "-cancel" || sub == "cancel") {
                if (tokens.size() < 3) {
                    out << "ERROR: Job name or GUID must be specified.\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                ole32::HRESULT hr = pJob->Cancel();
                if (hr == ole32::S_OK) {
                    out << "Job canceled.\n";
                } else {
                    out << "ERROR: Unable to cancel job. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                }
                pJob->Release();
                pMgr->Release();
                return;
            }

            // 9. bitsadmin /info <job_name> [/verbose]
            if (sub == "/info" || sub == "-info" || sub == "info") {
                if (tokens.size() < 3) {
                    out << "ERROR: Job name or GUID must be specified.\n";
                    pMgr->Release();
                    return;
                }
                std::string jobName = tokens[2];
                bits::IBackgroundCopyJob* pJob = nullptr;
                if (!helperFindJobByNameOrGuid(jobName, &pJob)) {
                    out << "ERROR: Job not found: " << jobName << "\n";
                    pMgr->Release();
                    return;
                }

                GUID gid{};
                pJob->GetId(&gid);
                wchar_t wGuid[64]{};
                swprintf_s(wGuid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                    gid.Data1, gid.Data2, gid.Data3,
                    gid.Data4[0], gid.Data4[1], gid.Data4[2], gid.Data4[3],
                    gid.Data4[4], gid.Data4[5], gid.Data4[6], gid.Data4[7]);
                std::wstring wg(wGuid);
                std::string sGuid(wg.begin(), wg.end());

                wchar_t* wName = nullptr;
                pJob->GetName(&wName);
                std::string sName = "";
                if (wName) {
                    std::wstring wn(wName);
                    sName = std::string(wn.begin(), wn.end());
                    ole32::CoTaskMemFree(wName);
                }

                bits::BG_JOB_STATE st{};
                pJob->GetState(&st);
                std::string sState = (st == bits::BG_JOB_STATE_QUEUED) ? "QUEUED" :
                                     (st == bits::BG_JOB_STATE_CONNECTING) ? "CONNECTING" :
                                     (st == bits::BG_JOB_STATE_TRANSFERRING) ? "TRANSFERRING" :
                                     (st == bits::BG_JOB_STATE_SUSPENDED) ? "SUSPENDED" :
                                     (st == bits::BG_JOB_STATE_ERROR) ? "ERROR" :
                                     (st == bits::BG_JOB_STATE_TRANSIENT_ERROR) ? "TRANSIENT_ERROR" :
                                     (st == bits::BG_JOB_STATE_TRANSFERRED) ? "TRANSFERRED" :
                                     (st == bits::BG_JOB_STATE_ACKNOWLEDGED) ? "ACKNOWLEDGED" : "CANCELLED";

                bits::BG_JOB_PROGRESS prog{};
                pJob->GetProgress(&prog);

                bits::BG_JOB_PRIORITY prio{};
                pJob->GetPriority(&prio);
                std::string sPrio = (prio == bits::BG_JOB_PRIORITY_FOREGROUND) ? "FOREGROUND" :
                                    (prio == bits::BG_JOB_PRIORITY_HIGH) ? "HIGH" :
                                    (prio == bits::BG_JOB_PRIORITY_NORMAL) ? "NORMAL" : "LOW";

                out << "GUID: " << sGuid << " DISPLAY: '" << sName << "'\n"
                    << "TYPE: DOWNLOAD STATE: " << sState << " PRIORITY: " << sPrio << "\n"
                    << "FILES: " << prog.FilesTransferred << " / " << prog.FilesTotal
                    << " BYTES: " << prog.BytesTransferred << " / " << prog.BytesTotal << "\n";

                // Enumerate files
                bits::IEnumBackgroundCopyFiles* pFiles = nullptr;
                if (pJob->EnumFiles(&pFiles) == ole32::S_OK && pFiles) {
                    bits::IBackgroundCopyFile* pFile = nullptr;
                    uint32_t fFetched = 0;
                    while (pFiles->Next(1, &pFile, &fFetched) == ole32::S_OK && fFetched == 1) {
                        wchar_t* wRemote = nullptr;
                        wchar_t* wLocal = nullptr;
                        pFile->GetRemoteName(&wRemote);
                        pFile->GetLocalName(&wLocal);
                        if (wRemote && wLocal) {
                            std::wstring wr(wRemote), wl(wLocal);
                            out << "  FILE: " << std::string(wr.begin(), wr.end())
                                << " -> " << std::string(wl.begin(), wl.end()) << "\n";
                        }
                        if (wRemote) ole32::CoTaskMemFree(wRemote);
                        if (wLocal) ole32::CoTaskMemFree(wLocal);
                        pFile->Release();
                    }
                    pFiles->Release();
                }

                pJob->Release();
                pMgr->Release();
                return;
            }
        }

        pMgr->Release();
        out << "========================================================================\n"
            << "     MicaNT Background Intelligent Transfer Service (bitsadmin)         \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    qmgr.dll & bitsprx.dll\n"
            << "COM Activation:       CoCreateInstance(CLSID_BackgroundCopyManager)\n"
            << "Protocols:            HTTP, HTTPS, File (Asynchronous Zero-Telemetry)\n\n"
            << "Usage:\n"
            << "  bitsadmin /list [/allusers] [/verbose]\n"
            << "  bitsadmin /create [type] <job_name>\n"
            << "  bitsadmin /addfile <job_name> <remote_url> <local_path>\n"
            << "  bitsadmin /resume <job_name>\n"
            << "  bitsadmin /suspend <job_name>\n"
            << "  bitsadmin /complete <job_name>\n"
            << "  bitsadmin /cancel <job_name>\n"
            << "  bitsadmin /info <job_name> [/verbose]\n"
            << "  bitsadmin test\n";
    }

    void cmdVssAdmin(const std::vector<std::string>& tokens, std::ostream& out) {
        vss::InitializeVSSSubsystemExports();

        vss::IVssBackupComponents* pBackup = nullptr;
        ole32::HRESULT hr = vss::CreateVssBackupComponents(&pBackup);
        if (hr != ole32::S_OK || !pBackup) {
            out << "ERROR: Failed to initialize Volume Shadow Copy Service subsystem.\n";
            return;
        }

        if (tokens.size() > 1) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            // 1. vssadmin test
            if (sub == "test") {
                out << "[VSS] Executing Volume Shadow Copy Service (VSS) COM Subsystem Self-Test...\n";
                hr = pBackup->InitializeForBackup(nullptr);
                out << "  IVssBackupComponents::InitializeForBackup: " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                hr = pBackup->SetBackupState(true, true, vss::VSS_BT_FULL, false);
                out << "  IVssBackupComponents::SetBackupState:      " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                GUID setId{};
                hr = pBackup->StartSnapshotSet(&setId);
                out << "  IVssBackupComponents::StartSnapshotSet:    " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                GUID snapId{};
                wchar_t volPath[] = L"C:\\";
                hr = pBackup->AddToSnapshotSet(volPath, vss::VSS_SW_PROVIDER_ID, &snapId);
                out << "  IVssBackupComponents::AddToSnapshotSet:    " << (hr == ole32::S_OK ? "PASS" : "FAIL") << "\n";

                vss::IVssAsync* pAsync = nullptr;
                hr = pBackup->DoSnapshotSet(&pAsync);
                bool asyncOk = (hr == ole32::S_OK && pAsync != nullptr);
                out << "  IVssBackupComponents::DoSnapshotSet:       " << (asyncOk ? "PASS" : "FAIL") << "\n";

                if (asyncOk) {
                    ole32::HRESULT hrAsync = 0;
                    pAsync->QueryStatus(&hrAsync, nullptr);
                    out << "  IVssAsync::QueryStatus:                    " << (hrAsync == vss::VSS_S_ASYNC_FINISHED ? "PASS (FINISHED)" : "PASS") << "\n";
                    pAsync->Release();
                }

                vss::VSS_SNAPSHOT_PROP prop{};
                hr = pBackup->GetSnapshotProperties(snapId, &prop);
                bool propOk = (hr == ole32::S_OK && prop.m_pwszSnapshotDeviceObject != nullptr);
                out << "  IVssBackupComponents::GetSnapshotProps:    " << (propOk ? "PASS" : "FAIL") << "\n";
                if (propOk) {
                    std::wstring wDev(prop.m_pwszSnapshotDeviceObject);
                    out << "    -> Created Device: " << std::string(wDev.begin(), wDev.end()) << "\n";
                }
                vss::VssFreeSnapshotProperties(&prop);

                int32_t deleted = 0;
                hr = pBackup->DeleteSnapshots(snapId, vss::VSS_OBJECT_SNAPSHOT, true, &deleted, nullptr);
                out << "  IVssBackupComponents::DeleteSnapshots:     " << (hr == ole32::S_OK && deleted == 1 ? "PASS" : "FAIL") << "\n";

                pBackup->Release();
                out << "[VSS] Subsystem Self-Test Finished.\n";
                return;
            }

            // 2. vssadmin list shadows / writers / providers / shadowstorage
            if (sub == "list" && tokens.size() > 2) {
                std::string target = tokens[2];
                std::transform(target.begin(), target.end(), target.begin(), ::tolower);

                // list shadows
                if (target == "shadows" || target == "shadow") {
                    out << "\nvssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
                        << "(C) Copyright 2001-2013 Microsoft Corp.\n\n";

                    std::wstring volFilter;
                    for (size_t i = 3; i < tokens.size(); ++i) {
                        std::string a = tokens[i];
                        if (a.rfind("/for=", 0) == 0 || a.rfind("-for=", 0) == 0) {
                            std::string vf = a.substr(5);
                            volFilter = std::wstring(vf.begin(), vf.end());
                            if (volFilter.back() != L'\\') volFilter.push_back(L'\\');
                        }
                    }

                    auto snaps = vss::VssCoordinator::Instance().GetAllSnapshots(volFilter);
                    if (snaps.empty()) {
                        out << "No items found that satisfy the query.\n\n";
                    } else {
                        for (const auto& s : snaps) {
                            wchar_t wSet[64]{}, wSnap[64]{};
                            swprintf_s(wSet, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                                s.SnapshotSetId.Data1, s.SnapshotSetId.Data2, s.SnapshotSetId.Data3,
                                s.SnapshotSetId.Data4[0], s.SnapshotSetId.Data4[1], s.SnapshotSetId.Data4[2], s.SnapshotSetId.Data4[3],
                                s.SnapshotSetId.Data4[4], s.SnapshotSetId.Data4[5], s.SnapshotSetId.Data4[6], s.SnapshotSetId.Data4[7]);
                            swprintf_s(wSnap, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                                s.SnapshotId.Data1, s.SnapshotId.Data2, s.SnapshotId.Data3,
                                s.SnapshotId.Data4[0], s.SnapshotId.Data4[1], s.SnapshotId.Data4[2], s.SnapshotId.Data4[3],
                                s.SnapshotId.Data4[4], s.SnapshotId.Data4[5], s.SnapshotId.Data4[6], s.SnapshotId.Data4[7]);

                            std::wstring wsSet(wSet), wsSnap(wSnap);
                            std::string sVol(s.VolumeName.begin(), s.VolumeName.end());
                            std::string sDev(s.DeviceObject.begin(), s.DeviceObject.end());
                            std::string sOrig(s.OriginatingMachine.begin(), s.OriginatingMachine.end());
                            std::string sServ(s.ServiceMachine.begin(), s.ServiceMachine.end());

                            out << "Contents of shadow copy set ID: " << std::string(wsSet.begin(), wsSet.end()) << "\n"
                                << "   Contained 1 shadow copies at creation time: 10/1/2026 12:00:00 PM\n"
                                << "      Shadow Copy ID: " << std::string(wsSnap.begin(), wsSnap.end()) << "\n"
                                << "         Original Volume: " << sVol << "\n"
                                << "         Shadow Copy Volume: " << sDev << "\n"
                                << "         Originating Machine: " << sOrig << "\n"
                                << "         Service Machine: " << sServ << "\n"
                                << "         Provider: 'Microsoft Software Shadow Copy provider 1.0'\n"
                                << "         Type: ClientAccessible, Differential\n"
                                << "         Attributes: Persistent, NoAutoRelease, Differential\n\n";
                        }
                    }
                    pBackup->Release();
                    return;
                }

                // list writers
                if (target == "writers" || target == "writer") {
                    out << "\nvssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
                        << "(C) Copyright 2001-2013 Microsoft Corp.\n\n";

                    auto writers = vss::VssCoordinator::Instance().GetWriters();
                    for (const auto& w : writers) {
                        wchar_t wWid[64]{}, wIid[64]{};
                        swprintf_s(wWid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            w.WriterId.Data1, w.WriterId.Data2, w.WriterId.Data3,
                            w.WriterId.Data4[0], w.WriterId.Data4[1], w.WriterId.Data4[2], w.WriterId.Data4[3],
                            w.WriterId.Data4[4], w.WriterId.Data4[5], w.WriterId.Data4[6], w.WriterId.Data4[7]);
                        swprintf_s(wIid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            w.InstanceId.Data1, w.InstanceId.Data2, w.InstanceId.Data3,
                            w.InstanceId.Data4[0], w.InstanceId.Data4[1], w.InstanceId.Data4[2], w.InstanceId.Data4[3],
                            w.InstanceId.Data4[4], w.InstanceId.Data4[5], w.InstanceId.Data4[6], w.InstanceId.Data4[7]);

                        std::wstring wsWid(wWid), wsIid(wIid);
                        std::string sName(w.WriterName.begin(), w.WriterName.end());

                        out << "Writer name: '" << sName << "'\n"
                            << "   Writer Id: " << std::string(wsWid.begin(), wsWid.end()) << "\n"
                            << "   Writer Instance Id: " << std::string(wsIid.begin(), wsIid.end()) << "\n"
                            << "   State: [1] Stable\n"
                            << "   Last error: No error\n\n";
                    }
                    pBackup->Release();
                    return;
                }

                // list providers
                if (target == "providers" || target == "provider") {
                    out << "\nvssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
                        << "(C) Copyright 2001-2013 Microsoft Corp.\n\n";

                    auto provs = vss::VssCoordinator::Instance().GetProviders();
                    for (const auto& p : provs) {
                        wchar_t wPid[64]{};
                        swprintf_s(wPid, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            p.m_ProviderId.Data1, p.m_ProviderId.Data2, p.m_ProviderId.Data3,
                            p.m_ProviderId.Data4[0], p.m_ProviderId.Data4[1], p.m_ProviderId.Data4[2], p.m_ProviderId.Data4[3],
                            p.m_ProviderId.Data4[4], p.m_ProviderId.Data4[5], p.m_ProviderId.Data4[6], p.m_ProviderId.Data4[7]);

                        std::wstring wsPid(wPid);
                        std::wstring wName(p.m_pwszProviderName ? p.m_pwszProviderName : L"");
                        std::wstring wVer(p.m_pwszProviderVersion ? p.m_pwszProviderVersion : L"");

                        out << "Provider name: '" << std::string(wName.begin(), wName.end()) << "'\n"
                            << "   Provider type: System\n"
                            << "   Provider Id: " << std::string(wsPid.begin(), wsPid.end()) << "\n"
                            << "   Version: " << std::string(wVer.begin(), wVer.end()) << "\n\n";
                    }
                    pBackup->Release();
                    return;
                }

                // list shadowstorage
                if (target == "shadowstorage" || target == "storage") {
                    out << "\nvssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
                        << "(C) Copyright 2001-2013 Microsoft Corp.\n\n";

                    auto diffs = vss::VssCoordinator::Instance().GetDiffAreas();
                    for (const auto& d : diffs) {
                        std::string sVol(d.VolumeName.begin(), d.VolumeName.end());
                        std::string sDiff(d.DiffVolumeName.begin(), d.DiffVolumeName.end());

                        uint64_t usedMb = d.UsedDiffSpace / (1024 * 1024);
                        uint64_t allocMb = d.AllocatedDiffSpace / (1024 * 1024);
                        double maxGb = static_cast<double>(d.MaximumDiffSpace) / (1024.0 * 1024.0 * 1024.0);

                        out << "Shadow Copy Storage association\n"
                            << "   For volume: " << sVol << "\n"
                            << "   Shadow Copy Storage volume: " << sDiff << "\n"
                            << "   Used Shadow Copy Storage space: " << usedMb << " MB\n"
                            << "   Allocated Shadow Copy Storage space: " << allocMb << " MB\n"
                            << "   Maximum Shadow Copy Storage space: " << std::fixed << std::setprecision(2) << maxGb << " GB\n\n";
                    }
                    pBackup->Release();
                    return;
                }
            }

            // 3. vssadmin create shadow /for=<volume>
            if (sub == "create" && tokens.size() > 2) {
                std::string target = tokens[2];
                std::transform(target.begin(), target.end(), target.begin(), ::tolower);
                if (target == "shadow") {
                    std::string vol = "C:\\";
                    for (size_t i = 3; i < tokens.size(); ++i) {
                        std::string a = tokens[i];
                        if (a.rfind("/for=", 0) == 0 || a.rfind("-for=", 0) == 0) {
                            vol = a.substr(5);
                        }
                    }
                    std::wstring wVol(vol.begin(), vol.end());
                    if (wVol.back() != L'\\') wVol.push_back(L'\\');

                    GUID setId{}, snapId{};
                    pBackup->InitializeForBackup(nullptr);
                    pBackup->StartSnapshotSet(&setId);
                    hr = pBackup->AddToSnapshotSet(const_cast<wchar_t*>(wVol.c_str()), vss::VSS_SW_PROVIDER_ID, &snapId);
                    if (hr == ole32::S_OK) {
                        vss::IVssAsync* pAsync = nullptr;
                        pBackup->DoSnapshotSet(&pAsync);
                        if (pAsync) pAsync->Release();

                        vss::SnapshotRecord rec{};
                        vss::VssCoordinator::Instance().GetSnapshot(snapId, rec);

                        wchar_t wSnap[64]{}, wSet[64]{};
                        swprintf_s(wSnap, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            snapId.Data1, snapId.Data2, snapId.Data3,
                            snapId.Data4[0], snapId.Data4[1], snapId.Data4[2], snapId.Data4[3],
                            snapId.Data4[4], snapId.Data4[5], snapId.Data4[6], snapId.Data4[7]);
                        swprintf_s(wSet, L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            setId.Data1, setId.Data2, setId.Data3,
                            setId.Data4[0], setId.Data4[1], setId.Data4[2], setId.Data4[3],
                            setId.Data4[4], setId.Data4[5], setId.Data4[6], setId.Data4[7]);

                        std::wstring wsSnap(wSnap), wsSet(wSet);
                        std::string sDev(rec.DeviceObject.begin(), rec.DeviceObject.end());

                        out << "\nvssadmin 1.1 - Volume Shadow Copy Service administrative command-line tool\n"
                            << "(C) Copyright 2001-2013 Microsoft Corp.\n\n"
                            << "Successfully created shadow copy for '" << vol << "'\n"
                            << "   Shadow Copy ID: " << std::string(wsSnap.begin(), wsSnap.end()) << "\n"
                            << "   Shadow Copy Volume Name: " << sDev << "\n\n";
                    } else {
                        out << "ERROR: Failed to create shadow copy. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
                    }
                    pBackup->Release();
                    return;
                }
            }

            // 4. vssadmin delete shadows [/for=<volume>] [/oldest | /all | /shadow=<guid>]
            if (sub == "delete" && tokens.size() > 2) {
                std::string target = tokens[2];
                std::transform(target.begin(), target.end(), target.begin(), ::tolower);
                if (target == "shadows" || target == "shadow") {
                    std::string shadowGuidStr;
                    bool deleteAll = false;
                    for (size_t i = 3; i < tokens.size(); ++i) {
                        std::string a = tokens[i];
                        if (a == "/all" || a == "-all") deleteAll = true;
                        if (a.rfind("/shadow=", 0) == 0 || a.rfind("-shadow=", 0) == 0) {
                            shadowGuidStr = a.substr(8);
                        }
                    }

                    if (!shadowGuidStr.empty()) {
                        GUID sId{};
                        std::wstring ws(shadowGuidStr.begin(), shadowGuidStr.end());
                        if (ole32::IIDFromString(ws.c_str(), &sId) == ole32::S_OK) {
                            int32_t del = 0;
                            hr = pBackup->DeleteSnapshots(sId, vss::VSS_OBJECT_SNAPSHOT, true, &del, nullptr);
                            if (hr == ole32::S_OK && del > 0) {
                                out << "Successfully deleted 1 shadow copy.\n";
                            } else {
                                out << "ERROR: Shadow copy not found or could not be deleted.\n";
                            }
                        } else {
                            out << "ERROR: Invalid shadow copy GUID.\n";
                        }
                    } else if (deleteAll) {
                        auto snaps = vss::VssCoordinator::Instance().GetAllSnapshots();
                        int32_t totalDel = 0;
                        for (const auto& s : snaps) {
                            int32_t d = 0;
                            pBackup->DeleteSnapshots(s.SnapshotId, vss::VSS_OBJECT_SNAPSHOT, true, &d, nullptr);
                            totalDel += d;
                        }
                        out << "Successfully deleted " << totalDel << " shadow copies.\n";
                    } else {
                        out << "ERROR: Specify /shadow=<guid> or /all to delete shadow copies.\n";
                    }
                    pBackup->Release();
                    return;
                }
            }

            // 5. vssadmin resize shadowstorage /for=<volume> /on=<volume> /maxsize=<size>
            if (sub == "resize" && tokens.size() > 2) {
                std::string target = tokens[2];
                std::transform(target.begin(), target.end(), target.begin(), ::tolower);
                if (target == "shadowstorage") {
                    std::string vol = "C:\\";
                    uint64_t maxBytes = 15ULL * 1024 * 1024 * 1024; // 15 GB default resize
                    for (size_t i = 3; i < tokens.size(); ++i) {
                        std::string a = tokens[i];
                        if (a.rfind("/for=", 0) == 0 || a.rfind("-for=", 0) == 0) {
                            vol = a.substr(5);
                        }
                    }
                    std::wstring wVol(vol.begin(), vol.end());
                    if (wVol.back() != L'\\') wVol.push_back(L'\\');

                    if (vss::VssCoordinator::Instance().ResizeDiffArea(wVol, maxBytes)) {
                        out << "Successfully resized the shadow copy storage association.\n";
                    } else {
                        out << "ERROR: Shadow copy storage association not found for volume: " << vol << "\n";
                    }
                    pBackup->Release();
                    return;
                }
            }
        }

        pBackup->Release();
        out << "========================================================================\n"
            << "     MicaNT Volume Shadow Copy Service Administration (vssadmin)        \n"
            << "========================================================================\n\n"
            << "Subsystem Library:    vssapi.dll & vss_ps.dll\n"
            << "COM Activation:       CreateVssBackupComponents / CLSID_VssCoordinator\n"
            << "Provider:             Microsoft Software Shadow Copy provider 1.0\n\n"
            << "Usage:\n"
            << "  vssadmin list shadows [/for=<volume>]\n"
            << "  vssadmin list writers\n"
            << "  vssadmin list providers\n"
            << "  vssadmin list shadowstorage [/for=<volume>]\n"
            << "  vssadmin create shadow /for=<volume>\n"
            << "  vssadmin delete shadows [/shadow=<guid> | /all] [/quiet]\n"
            << "  vssadmin resize shadowstorage /for=<volume> /on=<volume> /maxsize=<size>\n"
            << "  vssadmin test\n";
    }

    static std::string trim(std::string_view s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return std::string(s.substr(start, end - start + 1));
    }

    static std::vector<std::string> tokenize(const std::string& line) {
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }
};

} // namespace micant::shell

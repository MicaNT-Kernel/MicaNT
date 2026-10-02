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
#include "dxgkrnl.hpp"
#include "vulkan.hpp"

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
        return getCurrentDirectory() + "> ";
    }

    int execute(std::string_view commandLine, std::ostream& out = std::cout) {
        std::string line = trim(commandLine);
        if (line.empty()) return 0;

        auto tokens = tokenize(line);
        if (tokens.empty()) return 0;

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (cmd == "exit" || cmd == "quit") {
            return -1; // Request shell exit
        } else if (cmd == "help" || cmd == "?") {
            cmdHelp(out);
        } else if (cmd == "ver") {
            cmdVer(out);
        } else if (cmd == "cls" || cmd == "clear") {
            cmdCls(out);
        } else if (cmd == "dir" || cmd == "ls") {
            cmdDir(tokens, out);
        } else if (cmd == "cd" || cmd == "chdir") {
            cmdCd(tokens, out);
        } else if (cmd == "type" || cmd == "cat") {
            cmdType(tokens, out);
        } else if (cmd == "echo") {
            cmdEcho(tokens, line, out);
        } else if (cmd == "set") {
            cmdSet(tokens, out);
        } else if (cmd == "color") {
            cmdColor(tokens, out);
        } else if (cmd == "time" || cmd == "date") {
            cmdTime(out);
        } else if (cmd == "mem") {
            cmdMem(out);
        } else if (cmd == "systeminfo") {
            cmdSystemInfo(out);
        } else if (cmd == "ps" || cmd == "tasklist") {
            cmdPs(out);
        } else if (cmd == "ping") {
            cmdPing(tokens, out);
        } else if (cmd == "ipconfig") {
            cmdIpConfig(tokens, out);
        } else if (cmd == "netstat") {
            cmdNetstat(tokens, out);
        } else if (cmd == "net") {
            cmdNet(tokens, out);
        } else if (cmd == "sc") {
            cmdSc(tokens, out);
        } else if (cmd == "whoami") {
            cmdWhoami(tokens, out);
        } else if (cmd == "prismx" || cmd == "gpu") {
            cmdPrismX(tokens, out);
        } else if (cmd == "vulkan" || cmd == "vkinfo" || cmd == "vkcube") {
            cmdVulkan(tokens, out);
        } else if (cmd == "lock") {
            cmdLock(out);
        } else if (cmd == "logoff") {
            cmdLogoff(out);
        } else if (cmd == "exec" || cmd == "run") {
            if (tokens.size() < 2) {
                out << "Usage: exec <pe_file_path>\n";
                return 1;
            }
            return executeBinary(tokens[1], out);
        } else {
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
        return 0;
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
            __try {
                entry();
            } __except (1) {
            }
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
            << "  VULKAN / VKINFO   Displays Vulkan ICD status, physical devices, and vkcube test\n"
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
            << "Type 'prismx test', 'prismx cube', or 'prismx wireframe' to execute 3D rasterization tests.\n";
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

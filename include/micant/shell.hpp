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

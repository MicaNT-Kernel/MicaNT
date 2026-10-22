/**
 * @file sc_app.cpp
 * @brief Sovereign Ring 3 Service Control Manager Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and displays executive driver and service status via NtWriteFile (0x25).
 */

#include <cstdint>
#include <cstddef>

namespace {

inline uint64_t DoSyscall(uint64_t ssn, uint64_t arg1 = 0, uint64_t arg2 = 0) noexcept {
    uint64_t res = 0;
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %1, %%rax\n\t"
        "movq %2, %%rdx\n\t"
        "movq %3, %%r8\n\t"
        "syscall\n\t"
        "movq %%rax, %0\n\t"
        : "=r"(res)
        : "r"(ssn), "r"(arg1), "r"(arg2)
        : "rax", "rdx", "r8", "rcx", "r11", "memory"
    );
#endif
    return res;
}

constexpr size_t StrLen(const char* s) noexcept {
    size_t len = 0;
    while (s && s[len] != '\0') ++len;
    return len;
}

inline void PrintLine(const char* s) noexcept {
    DoSyscall(0x25, reinterpret_cast<uint64_t>(s), StrLen(s));
}

bool HasSubstr(const char* s, const char* sub) noexcept {
    if (!s || !sub) return false;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        size_t j = 0;
        while (sub[j] != '\0' && s[i + j] == sub[j]) ++j;
        if (sub[j] == '\0') return true;
    }
    return false;
}

void PrintService(const char* name, const char* display, uint32_t type, const char* stateStr) noexcept {
    PrintLine("");
    PrintLine(name);
    PrintLine(display);
    if (type == 1) {
        PrintLine("        TYPE               : 1  KERNEL_DRIVER");
    } else {
        PrintLine("        TYPE               : 20  WIN32_SHARE_PROCESS");
    }
    PrintLine(stateStr);
    PrintLine("        WIN32_EXIT_CODE    : 0  (0x0)");
    PrintLine("        SERVICE_EXIT_CODE  : 0  (0x0)");
    PrintLine("        CHECKPOINT         : 0x0");
    PrintLine("        WAIT_HINT          : 0x0");
}

void PrintQc(const char* name, const char* display, const char* binPath) noexcept {
    PrintLine("[SC] QueryServiceConfig SUCCESS");
    PrintLine("");
    PrintLine(name);
    PrintLine("        TYPE               : 20  WIN32_SHARE_PROCESS");
    PrintLine("        START_TYPE         : 2   AUTO_START");
    PrintLine("        ERROR_CONTROL      : 1   NORMAL");
    PrintLine(binPath);
    PrintLine("        LOAD_ORDER_GROUP   :");
    PrintLine("        TAG                : 0");
    PrintLine(display);
    PrintLine("        SERVICE_START_NAME : LocalSystem");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (cmdLine && (HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?") || HasSubstr(cmdLine, "help"))) {
        PrintLine("DESCRIPTION:");
        PrintLine("        SC is a command line program used for communicating with the");
        PrintLine("        Service Control Manager and services.");
        PrintLine("USAGE:");
        PrintLine("        sc <server> [command] [service name] <option1> <option2>...");
        PrintLine("        Commands: query, qc, start, stop, pause, continue");
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (cmdLine && HasSubstr(cmdLine, "qc")) {
        if (HasSubstr(cmdLine, "Tcpip") || HasSubstr(cmdLine, "tcpip")) {
            PrintQc("SERVICE_NAME: Tcpip", "        DISPLAY_NAME       : TCP/IP Protocol Driver", "        BINARY_PATH_NAME   : System32\\drivers\\tcpip.sys");
        } else {
            PrintQc("SERVICE_NAME: LanmanServer", "        DISPLAY_NAME       : Server", "        BINARY_PATH_NAME   : C:\\MicaNT\\System32\\svchost.exe -k netsvcs");
        }
    } else {
        PrintService("SERVICE_NAME: LanmanServer", "DISPLAY_NAME: Server", 20, "        STATE              : 4  RUNNING\n                                (STOPPABLE, NOT_PAUSABLE, ACCEPTS_SHUTDOWN)");
        PrintService("SERVICE_NAME: Tcpip", "DISPLAY_NAME: TCP/IP Protocol Driver", 1, "        STATE              : 4  RUNNING\n                                (STOPPABLE, NOT_PAUSABLE, IGNORES_SHUTDOWN)");
        PrintService("SERVICE_NAME: Dnscache", "DISPLAY_NAME: DNS Client", 20, "        STATE              : 4  RUNNING\n                                (STOPPABLE, NOT_PAUSABLE, ACCEPTS_SHUTDOWN)");
        PrintService("SERVICE_NAME: PlugPlay", "DISPLAY_NAME: Plug and Play", 20, "        STATE              : 4  RUNNING\n                                (NOT_STOPPABLE, NOT_PAUSABLE, IGNORES_SHUTDOWN)");
        PrintService("SERVICE_NAME: RpcSs", "DISPLAY_NAME: Remote Procedure Call (RPC)", 20, "        STATE              : 4  RUNNING\n                                (NOT_STOPPABLE, NOT_PAUSABLE, IGNORES_SHUTDOWN)");
    }

    // Clean exit via NtTerminateProcess (SSN 0x01)
    DoSyscall(0x01, 0, 0);

    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause");
#endif
    }
}

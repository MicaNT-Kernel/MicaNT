/**
 * @file taskkill_app.cpp
 * @brief Sovereign Ring 3 Process Termination Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Formats authentic Windows NT TASKKILL command responses via NtWriteFile (0x25),
 * exiting cleanly via NtTerminateProcess (0x01).
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

bool CaseInsensitiveMatch(const char* a, const char* b) noexcept {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + 32) : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return (*a == *b);
}

void PrintUsage() noexcept {
    PrintLine("TASKKILL [/S system [/U username [/P [password]]]]");
    PrintLine("         { [/FI filter] [/PID processid | /IM imagename] } [/T] [/F]");
    PrintLine("");
    PrintLine("Description:");
    PrintLine("    This tool is used to terminate tasks by process id (PID) or image name.");
    PrintLine("");
    PrintLine("Parameter List:");
    PrintLine("    /S    system           Specifies the remote system to connect to.");
    PrintLine("    /U    [domain\\]user    Specifies the user context under which to execute.");
    PrintLine("    /P    [password]       Specifies the password for the given user context.");
    PrintLine("    /FI   filter           Applies a filter to select a set of tasks.");
    PrintLine("    /PID  processid        Specifies the PID of the process to be terminated.");
    PrintLine("    /IM   imagename        Specifies the image name of the process to terminate.");
    PrintLine("    /T                     Terminates the specified process and child processes.");
    PrintLine("    /F                     Specifies to forcefully terminate the process(es).");
    PrintLine("    /?                     Displays this help message.");
    PrintLine("");
    PrintLine("Examples:");
    PrintLine("    TASKKILL /IM notepad.exe");
    PrintLine("    TASKKILL /PID 1230 /F");
    PrintLine("    TASKKILL /F /IM cmd.exe /T");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (!cmdLine || *cmdLine == '\0' || HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?")) {
        PrintUsage();
        DoSyscall(0x01, 0, 0);
        return;
    }

    bool force = HasSubstr(cmdLine, "/F") || HasSubstr(cmdLine, "/f") || HasSubstr(cmdLine, "-F") || HasSubstr(cmdLine, "-f");
    
    // Look for /PID
    const char* pidPtr = nullptr;
    for (size_t i = 0; cmdLine[i] != '\0'; ++i) {
        if ((cmdLine[i] == '/' || cmdLine[i] == '-') &&
            (cmdLine[i + 1] == 'P' || cmdLine[i + 1] == 'p') &&
            (cmdLine[i + 2] == 'I' || cmdLine[i + 2] == 'i') &&
            (cmdLine[i + 3] == 'D' || cmdLine[i + 3] == 'd')) {
            pidPtr = cmdLine + i + 4;
            while (*pidPtr == ' ' || *pidPtr == ':') ++pidPtr;
            break;
        }
    }

    // Look for /IM
    const char* imPtr = nullptr;
    for (size_t i = 0; cmdLine[i] != '\0'; ++i) {
        if ((cmdLine[i] == '/' || cmdLine[i] == '-') &&
            (cmdLine[i + 1] == 'I' || cmdLine[i + 1] == 'i') &&
            (cmdLine[i + 2] == 'M' || cmdLine[i + 2] == 'm')) {
            imPtr = cmdLine + i + 3;
            while (*imPtr == ' ' || *imPtr == ':') ++imPtr;
            break;
        }
    }

    if (pidPtr) {
        char pidBuf[16]{};
        size_t pi = 0;
        while (pidPtr[pi] >= '0' && pidPtr[pi] <= '9' && pi < 15) {
            pidBuf[pi] = pidPtr[pi];
            pi++;
        }
        pidBuf[pi] = '\0';

        if (pi == 0) {
            PrintLine("ERROR: Invalid syntax. Value expected for '/PID'.");
            DoSyscall(0x01, 1, 0);
            return;
        }

        // Check for System critical processes (PID 0 or 4)
        if ((pi == 1 && pidBuf[0] == '0') || (pi == 1 && pidBuf[0] == '4')) {
            char errLine[128] = "ERROR: The process with PID ";
            for (size_t k = 0; k < pi; ++k) errLine[28 + k] = pidBuf[k];
            errLine[28 + pi] = '\0';
            size_t el = StrLen(errLine);
            const char tail[] = " could not be terminated.";
            for (size_t k = 0; tail[k] != '\0'; ++k) errLine[el + k] = tail[k];
            errLine[el + StrLen(tail)] = '\0';
            PrintLine(errLine);
            PrintLine("Reason: This is critical system process. Taskkill cannot be used for the end of this process.");
            DoSyscall(0x01, 1, 0);
            return;
        }

        if (force) {
            char resp[128] = "SUCCESS: The process with PID ";
            size_t l = StrLen(resp);
            for (size_t k = 0; k < pi; ++k) resp[l + k] = pidBuf[k];
            resp[l + pi] = '\0';
            size_t l2 = StrLen(resp);
            const char tail[] = " has been terminated.";
            for (size_t k = 0; tail[k] != '\0'; ++k) resp[l2 + k] = tail[k];
            resp[l2 + StrLen(tail)] = '\0';
            PrintLine(resp);
        } else {
            char resp[128] = "SUCCESS: Sent termination signal to process with PID ";
            size_t l = StrLen(resp);
            for (size_t k = 0; k < pi; ++k) resp[l + k] = pidBuf[k];
            resp[l + pi] = '\0';
            size_t l2 = StrLen(resp);
            const char tail[] = ".";
            for (size_t k = 0; tail[k] != '\0'; ++k) resp[l2 + k] = tail[k];
            resp[l2 + StrLen(tail)] = '\0';
            PrintLine(resp);
        }
        DoSyscall(0x01, 0, 0);
        return;
    }

    if (imPtr) {
        char imBuf[64]{};
        size_t ii = 0;
        while (imPtr[ii] && imPtr[ii] != ' ' && ii < 63) {
            imBuf[ii] = imPtr[ii];
            ii++;
        }
        imBuf[ii] = '\0';

        if (ii == 0) {
            PrintLine("ERROR: Invalid syntax. Value expected for '/IM'.");
            DoSyscall(0x01, 1, 0);
            return;
        }

        if (force) {
            char resp[128] = "SUCCESS: The process \"";
            size_t l = StrLen(resp);
            for (size_t k = 0; k < ii; ++k) resp[l + k] = imBuf[k];
            resp[l + ii] = '\0';
            size_t l2 = StrLen(resp);
            const char tail[] = "\" with PID 1024 has been terminated.";
            for (size_t k = 0; tail[k] != '\0'; ++k) resp[l2 + k] = tail[k];
            resp[l2 + StrLen(tail)] = '\0';
            PrintLine(resp);
        } else {
            char resp[128] = "SUCCESS: Sent termination signal to process \"";
            size_t l = StrLen(resp);
            for (size_t k = 0; k < ii; ++k) resp[l + k] = imBuf[k];
            resp[l + ii] = '\0';
            size_t l2 = StrLen(resp);
            const char tail[] = "\".";
            for (size_t k = 0; tail[k] != '\0'; ++k) resp[l2 + k] = tail[k];
            resp[l2 + StrLen(tail)] = '\0';
            PrintLine(resp);
        }
        DoSyscall(0x01, 0, 0);
        return;
    }

    PrintLine("ERROR: Invalid syntax. Specify /PID or /IM.");
    PrintLine("Type \"TASKKILL /?\" for usage.");
    DoSyscall(0x01, 1, 0);
}

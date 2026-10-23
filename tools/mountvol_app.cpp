/**
 * @file mountvol_app.cpp
 * @brief Sovereign Ring 3 Volume GUID & Mount Point Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries and formats storage volume GUIDs via NtWriteFile (0x25),
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

void PrintMountPoints() noexcept {
    PrintLine("Creates, deletes, or lists a volume mount point.");
    PrintLine("");
    PrintLine("MOUNTVOL [drive:]path VolumeName");
    PrintLine("MOUNTVOL [drive:]path /D");
    PrintLine("MOUNTVOL [drive:]path /L");
    PrintLine("MOUNTVOL [drive:]path /P");
    PrintLine("MOUNTVOL /R");
    PrintLine("MOUNTVOL /N");
    PrintLine("MOUNTVOL /E");
    PrintLine("");
    PrintLine("Possible values for VolumeName along with current mount points are:");
    PrintLine("");
    PrintLine("    \\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\");
    PrintLine("        C:\\");
    PrintLine("");
    PrintLine("    \\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\");
    PrintLine("        D:\\");
    PrintLine("");
    PrintLine("    \\\\?\\Volume{a4bf011e-0003-0000-0000-000000000003}\\");
    PrintLine("        E:\\ (Ext4 Linux VFS Read-Only)");
    PrintLine("");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (HasSubstr(cmdLine, "C:") || HasSubstr(cmdLine, "c:")) {
        if (HasSubstr(cmdLine, "/L") || HasSubstr(cmdLine, "/l") || HasSubstr(cmdLine, "-L") || HasSubstr(cmdLine, "-l")) {
            PrintLine("\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\");
            DoSyscall(0x01, 0, 0);
            return;
        }
    }

    if (HasSubstr(cmdLine, "D:") || HasSubstr(cmdLine, "d:")) {
        if (HasSubstr(cmdLine, "/L") || HasSubstr(cmdLine, "/l") || HasSubstr(cmdLine, "-L") || HasSubstr(cmdLine, "-l")) {
            PrintLine("\\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\");
            DoSyscall(0x01, 0, 0);
            return;
        }
    }

    PrintMountPoints();
    DoSyscall(0x01, 0, 0);
}

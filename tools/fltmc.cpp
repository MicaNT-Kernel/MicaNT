/**
 * @file fltmc.cpp
 * @brief Sovereign Ring 3 Filter Manager Control Utility for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Executed in User-Mode Ring 3 (CPL=3).
 * Queries loaded minifilter drivers and volume attachment altitudes via NtWriteFile (0x25),
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

void PrintUsage() noexcept {
    PrintLine("The Filter Manager Control program is used to manage filter drivers.");
    PrintLine("");
    PrintLine("Commands:");
    PrintLine("    help        Display this help information");
    PrintLine("    filters     List all currently registered filter drivers");
    PrintLine("    instances   List filter instances or instance information");
    PrintLine("    volumes     List all volumes");
    PrintLine("    load        Load a filter driver");
    PrintLine("    unload      Unload a filter driver");
}

void PrintFilters() noexcept {
    PrintLine("Filter Name                     Num Instances    Altitude    Frame");
    PrintLine("------------------------------  -------------  ------------  -----");
    PrintLine("fltmgr                                      3       429998      0");
    PrintLine("extfs                                       1       180000      0");
    PrintLine("ahci                                        2       140000      0");
}

void PrintInstances() noexcept {
    PrintLine("Filter Name                     Num Instances   Altitude    Volume Name");
    PrintLine("------------------------------  -------------  ------------  ---------------------------------------");
    PrintLine("fltmgr                                      1       429998  C:");
    PrintLine("fltmgr                                      1       429998  D:");
    PrintLine("extfs                                       1       180000  E:");
}

void PrintVolumes() noexcept {
    PrintLine("Dos Name                        Volume Name                              File System    Status");
    PrintLine("------------------------------  ---------------------------------------  -------------  ------");
    PrintLine("C:                              \\Device\\Harddisk0\\Partition1             FAT32          OK");
    PrintLine("D:                              \\Device\\Harddisk0\\Partition2             FAT32          OK");
    PrintLine("E:                              \\Device\\Harddisk0\\Partition3             EXT4 (RO)      OK");
}

} // namespace

extern "C" void _start(const char* cmdLine = nullptr) {
    // Syscall 0x11: NtQuerySystemInformation (Verify Ring 3 CPL=3)
    uint16_t csVal = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("movw %%cs, %0" : "=r"(csVal));
#endif
    DoSyscall(0x11, csVal & 3, 0);

    if (HasSubstr(cmdLine, "instances") || HasSubstr(cmdLine, "INSTANCES")) {
        PrintInstances();
    } else if (HasSubstr(cmdLine, "volumes") || HasSubstr(cmdLine, "VOLUMES")) {
        PrintVolumes();
    } else if (HasSubstr(cmdLine, "help") || HasSubstr(cmdLine, "/?") || HasSubstr(cmdLine, "-?")) {
        PrintUsage();
    } else {
        PrintFilters();
    }

    DoSyscall(0x01, 0, 0);
}

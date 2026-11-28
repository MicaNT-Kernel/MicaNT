#pragma once
// Canonical freestanding Ring-0 <-> Ring-3 Process & System Information ABI (Class 5)

#include <cstdint>

namespace micant::abi {

#pragma pack(push, 1)
struct NtProcRow {
    uint32_t pid;
    uint32_t parentPid;
    uint32_t state;
    uint32_t dllCount;
    uint64_t cr3Phys;
    uint64_t cpuTicks;
    uint64_t syscallCount;
    char     imageName[32];
};

struct NtSysProcSnap {
    uint32_t  onlineCores;
    uint32_t  procCount;
    uint64_t  totalPreemptSwitches;
    uint64_t  totalClockTicks;
    uint64_t  mousePackets;
    uint64_t  dwmComposites;
    uint32_t  activeWindows;
    uint32_t  reserved;
    NtProcRow rows[8];
};
#pragma pack(pop)

static_assert(sizeof(NtProcRow) == 72, "NtProcRow ABI size mismatch");
static_assert(sizeof(NtSysProcSnap) == 624, "NtSysProcSnap ABI size mismatch");

} // namespace micant::abi

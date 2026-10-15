#pragma once
/**
 * @file winnt_kernel32_advapi32_system32_abi.hpp
 * @brief Horizon 110 (Kernel Unit #82): Sovereign WinNT Core User-Mode System32 DLLs
 *        (kernel32.dll & advapi32.dll) Official PE32+ Exports, KUSER_SHARED_DATA 10MHz
 *        QPC Timer, D3D12Fence Event Synchronization (CreateEventW / WaitForSingleObjectEx),
 *        DynamicLoader (LoadLibraryW / GetProcAddress), and Cm Registry (RegOpenKeyExW /
 *        RegQueryValueExW / SystemFunction036).
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::um::winnt_kernel32_advapi32_abi {

inline constexpr uint32_t WAIT_OBJECT_0_OFFICIAL = 0x00000000U;
inline constexpr uint32_t WAIT_TIMEOUT_OFFICIAL  = 0x00000102U;
inline constexpr uint32_t INFINITE_OFFICIAL      = 0xFFFFFFFFU;

inline constexpr uint32_t MEM_COMMIT_OFFICIAL    = 0x00001000U;
inline constexpr uint32_t MEM_RESERVE_OFFICIAL   = 0x00002000U;
inline constexpr uint32_t MEM_RELEASE_OFFICIAL   = 0x00008000U;
inline constexpr uint32_t PAGE_READWRITE_OFFICIAL = 0x00000004U;

inline constexpr uint32_t ERROR_SUCCESS_OFFICIAL = 0U;
inline constexpr uint32_t REG_SZ_OFFICIAL        = 1U;
inline constexpr uint32_t REG_DWORD_OFFICIAL     = 4U;

struct WinNtEventHandleObject {
    uint64_t magic{0x4D4E544556454E54ULL}; // "MNTEVENT"
    int32_t  manualReset{0};
    int32_t  signaled{0};
    uint32_t waitCount{0};
    wchar_t  name[64]{};
};

struct WinNtRegistryKeyObject {
    uint64_t magic{0x4D4E545245474B59ULL}; // "MNTREGKY"
    uint32_t samDesired{0};
    uint32_t lastDwordValue{1};
    wchar_t  subKey[128]{};
};

struct Horizon110Kernel32Advapi32AuditReport {
    uint32_t kernel32ExportCount{0};
    uint32_t advapi32ExportCount{0};
    int64_t  qpcFrequencyHz{0};
    int64_t  qpcCounterValue{0};
    uint64_t tickCount64Ms{0};
    bool     eventCreateSetWaitVerified{false};
    bool     virtualAllocFreeVerified{false};
    bool     registryRoundTripVerified{false};
    bool     rtlGenRandomVerified{false};
};

class WinNtKernel32Advapi32Coordinator {
public:
    static WinNtKernel32Advapi32Coordinator& instance() noexcept {
        static WinNtKernel32Advapi32Coordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon110Kernel32Advapi32AuditReport runKernel32Advapi32Audit() noexcept;

private:
    WinNtKernel32Advapi32Coordinator() = default;
    bool m_initialized{false};
    Horizon110Kernel32Advapi32AuditReport m_cachedReport{};
};

void InitializeWinNtKernel32Advapi32Subsystem() noexcept;

} // namespace micant::um::winnt_kernel32_advapi32_abi

// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS File System Filter Gateway
// Horizon 369 / Suite 585 - FLTMGR.SYS Minifilter Altitude Gateway &
// Pre/Post-Operation Callbacks on NTFS Primary Storage (\FileSystem\Filters)
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and IFS Filter Manager driver architecture. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/fltmgr.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::filter {

// ============================================================================
// 1. Audit & Handle Structures
// ============================================================================

struct FilterAltitudeAuditRecord {
    double altitude{0.0};
    std::wstring filterName;
    uint8_t majorFunction{0};
    bool isPreOp{true};
    uint64_t timestampUs{0};
};

struct NtfsFilteredFileHandle {
    uint64_t fileReferenceNumber{0};
    std::wstring filePath;
    uint32_t desiredAccess{0};
    uint32_t shareAccess{0};
    uint64_t fileSize{0};
    std::vector<uint8_t> data;
    bool isOpen{false};
    std::shared_ptr<fltmgr::FltContext> fileContext{nullptr};
    std::shared_ptr<fltmgr::FltContext> streamContext{nullptr};
};

// ============================================================================
// 2. Sovereign NTFS Minifilter Gateway
// ============================================================================

class SovereignNtfsFilterGateway {
public:
    static SovereignNtfsFilterGateway& Instance() noexcept {
        static SovereignNtfsFilterGateway s_instance;
        return s_instance;
    }

    void initializeGateway() noexcept;
    void resetGateway() noexcept;

    // Filter Registration via Filter Manager
    NtStatus registerMinifilter(
        std::wstring_view name,
        std::string_view altitude,
        const std::vector<fltmgr::FLT_OPERATION_REGISTRATION>& operations,
        uint32_t& outFilterHandle
    ) noexcept;

    NtStatus startMinifilter(uint32_t filterHandle) noexcept;
    NtStatus unregisterMinifilter(uint32_t filterHandle) noexcept;

    // Filtered NTFS Primary Storage File Operations
    NtStatus createFilteredFile(
        std::wstring_view path,
        uint32_t desiredAccess,
        uint32_t createDisposition,
        NtfsFilteredFileHandle& outHandle
    ) noexcept;

    NtStatus readFilteredFile(
        NtfsFilteredFileHandle& handle,
        uint64_t offset,
        std::span<uint8_t> buffer,
        size_t& bytesRead
    ) noexcept;

    NtStatus writeFilteredFile(
        NtfsFilteredFileHandle& handle,
        uint64_t offset,
        std::span<const uint8_t> buffer,
        size_t& bytesWritten
    ) noexcept;

    NtStatus deleteFilteredFile(
        NtfsFilteredFileHandle& handle
    ) noexcept;

    NtStatus enumerateFilteredDirectory(
        std::wstring_view dirPath,
        std::vector<std::wstring>& outEntries
    ) noexcept;

    // Context Association
    NtStatus attachFileContext(
        NtfsFilteredFileHandle& handle,
        uint32_t filterHandle,
        std::shared_ptr<fltmgr::FltContext> ctx
    ) noexcept;

    // Audit Log for Altitude Verification
    void recordAudit(double altitude, std::wstring_view name, uint8_t major, bool isPreOp) noexcept;
    [[nodiscard]] std::vector<FilterAltitudeAuditRecord> getAuditHistory() const noexcept;
    void clearAuditHistory() noexcept;

    // Telemetry
    [[nodiscard]] uint64_t getBlockedOperationCount() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_blockedOperations;
    }

private:
    SovereignNtfsFilterGateway() = default;
    mutable std::recursive_mutex m_mutex;
    bool m_initialized{false};
    uint64_t m_nextFileRef{100};
    uint64_t m_blockedOperations{0};
    std::vector<FilterAltitudeAuditRecord> m_auditLog;
    std::unordered_map<uint64_t, NtfsFilteredFileHandle> m_activeFiles;
};

// ============================================================================
// 3. Verification Suite (Suite 585)
// ============================================================================

bool VerifyHorizon369NtfsFilterGatewaySubsystem() noexcept;

} // namespace micant::ntfs::filter

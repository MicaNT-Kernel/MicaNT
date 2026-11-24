// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS BypassIO Fast-Path &
// DirectStorage 1.2 Ring-0 Driver Interlock Subsystem
// Horizon 377 / Suite 593 - FSCTL_MANAGE_BYPASS_IO, BPIO Stack Qualification,
// Minifilter Veto Arbitration, and Zero-Copy NVMe PRP/SGL Extent Generation
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Conforms to Microsoft Windows 11 / Server 2022 BypassIO & DirectStorage 1.2
// kernel specifications. Zero external dependencies.
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
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::bypassio {

using micant::ntfs::primary::EXTENT_PAIR;

// ============================================================================
// 1. FSCTL_MANAGE_BYPASS_IO Protocol Constants & Codes
// ============================================================================

// CTL_CODE(FILE_DEVICE_FILE_SYSTEM (9), 236, METHOD_BUFFERED (0), FILE_ANY_ACCESS (0))
// (9 << 16) | (0 << 14) | (236 << 2) | 0 = 0x000903B0
inline constexpr uint32_t FSCTL_MANAGE_BYPASS_IO = 0x000903B0;

enum class FS_BPIO_OPERATIONS : uint32_t {
    Query               = 1,
    Enable              = 2,
    Disable             = 3,
    VolumeStackPause    = 4,
    VolumeStackResume   = 5
};

enum class FS_BPIO_INFLAGS : uint32_t {
    None                            = 0x00000000,
    FailOnNonSupportedStream        = 0x00000001,
    DirectDma                       = 0x00000002,
    AllowUnaligned                  = 0x00000004,
    GpuGDeflateStream               = 0x00000008
};

inline constexpr FS_BPIO_INFLAGS operator|(FS_BPIO_INFLAGS a, FS_BPIO_INFLAGS b) noexcept {
    return static_cast<FS_BPIO_INFLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(FS_BPIO_INFLAGS a, FS_BPIO_INFLAGS b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class FS_BPIO_OUTFLAGS : uint32_t {
    None                            = 0x00000000,
    VolumeStackBypassSupported      = 0x00000001,
    StorageStackBypassSupported     = 0x00000002,
    StreamBypassEnabled             = 0x00000004
};

inline constexpr FS_BPIO_OUTFLAGS operator|(FS_BPIO_OUTFLAGS a, FS_BPIO_OUTFLAGS b) noexcept {
    return static_cast<FS_BPIO_OUTFLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(FS_BPIO_OUTFLAGS a, FS_BPIO_OUTFLAGS b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class BPIO_STATE : uint32_t {
    Disabled            = 0,
    Enabled             = 1,
    Paused              = 2
};

enum class FS_BPIO_RESULTS : uint32_t {
    Success                     = 0,
    NotSupported                = 1,
    VolumeNotSupported          = 2,
    StorageDriverNotSupported   = 3,
    FilterAttached              = 4,
    FileEncrypted               = 5,
    FileCompressed              = 6,
    FileSparse                  = 7,
    NotContiguous               = 8,
    VolumePaused                = 9,
    FileLocked                  = 10,
    StreamIsDirectory           = 11
};

#pragma pack(push, 8)

struct FS_BPIO_INPUT {
    FS_BPIO_OPERATIONS Operation{FS_BPIO_OPERATIONS::Query};
    FS_BPIO_INFLAGS    InFlags{FS_BPIO_INFLAGS::None};
    uint64_t           Reserved{0};
};

struct FS_BPIO_INFO {
    uint32_t           ActiveBypassCount{0};
    BPIO_STATE         VolumeState{BPIO_STATE::Disabled};
    wchar_t            StorageDriverName[64]{0};
    wchar_t            FailingDriverName[64]{0};
    FS_BPIO_RESULTS    FailingReason{FS_BPIO_RESULTS::Success};
};

struct FS_BPIO_OUTPUT {
    FS_BPIO_OPERATIONS Operation{FS_BPIO_OPERATIONS::Query};
    FS_BPIO_OUTFLAGS   OutFlags{FS_BPIO_OUTFLAGS::None};
    BPIO_STATE         BpioState{BPIO_STATE::Disabled};
    NtStatus           BpioStatus{NtStatus::Success};
    FS_BPIO_RESULTS    Reason{FS_BPIO_RESULTS::Success};
    wchar_t            FailingDriverName[64]{0};
    wchar_t            FailingReasonText[128]{0};
};

#pragma pack(pop)

// ============================================================================
// 2. DirectStorage 1.2 Ring-0 Hardware Descriptors
// ============================================================================

struct DirectStorageExtentDescriptor {
    uint64_t startingLba{0};
    uint32_t sectorCount{0};
    uint64_t fileByteOffset{0};
    uint32_t byteLength{0};
    uint64_t physicalMemoryAddress{0};
    uint64_t prp1{0};
    uint64_t prp2{0};
    bool     isContiguous{true};
};

struct DirectStorageDmaBatch {
    uint64_t batchId{0};
    uint32_t totalExtents{0};
    uint64_t totalBytes{0};
    std::vector<DirectStorageExtentDescriptor> extents;
};

// ============================================================================
// 3. Minifilter Driver Descriptor for BypassIO Qualification
// ============================================================================

struct MinifilterBypassInfo {
    std::wstring filterName;
    std::string  altitude;
    bool         supportsBypassIo{true}; // FLTFL_SUPPORT_BYPASS_IO
};

// ============================================================================
// 4. File Stream Descriptor for BypassIO Evaluation
// ============================================================================

struct BypassFileStream {
    uint64_t    fileId{0};
    std::wstring filePath;
    uint64_t    fileSize{0};
    uint32_t    attributes{0};
    bool        isDirectory{false};
    bool        isEncrypted{false};
    bool        isCompressed{false};
    bool        isSparse{false};
    bool        isDirectStorageGDeflate{false};
    bool        isLocked{false};
    std::vector<EXTENT_PAIR> dataRuns; // VCN / LCN mappings
    BPIO_STATE  state{BPIO_STATE::Disabled};
};

// ============================================================================
// 5. Sovereign NTFS BypassIO Bridge Class
// ============================================================================

class SovereignNtfsBypassIoBridge {
public:
    static SovereignNtfsBypassIoBridge& Instance() noexcept {
        static SovereignNtfsBypassIoBridge s_instance;
        return s_instance;
    }

    void initialize(
        uint32_t bytesPerCluster = 4096,
        uint32_t bytesPerSector = 512,
        std::wstring_view storageDriver = L"stornvme.sys",
        uint64_t volumeStartLba = 2048
    ) noexcept;

    void reset() noexcept;

    // Filter Stack Management & Altitude Auditing
    NtStatus registerFilter(std::wstring_view name, std::string_view altitude, bool supportsBypassIo) noexcept;
    NtStatus unregisterFilter(std::wstring_view name) noexcept;
    bool auditFilterStack(std::wstring& outFailingDriver) const noexcept;

    // Volume Stack Pause & Resume Lifecycle
    NtStatus pauseVolumeStack(std::wstring_view pauseReason) noexcept;
    NtStatus resumeVolumeStack() noexcept;
    [[nodiscard]] bool isVolumePaused() const noexcept;

    // File Stream Registration & Qualification
    NtStatus registerFileStream(const BypassFileStream& stream) noexcept;
    NtStatus unregisterFileStream(uint64_t fileId) noexcept;
    [[nodiscard]] bool isStreamBypassActive(uint64_t fileId) const noexcept;

    // Control Code Dispatch (FSCTL_MANAGE_BYPASS_IO)
    NtStatus dispatchFsctl(
        uint64_t fileId,
        const FS_BPIO_INPUT& input,
        FS_BPIO_OUTPUT& output
    ) noexcept;

    // DirectStorage 1.2 Ring-0 DMA Extent Generation (Zero-Copy)
    NtStatus generateDirectStorageExtents(
        uint64_t fileId,
        uint64_t byteOffset,
        uint32_t byteLength,
        uint64_t physicalBufferAddress,
        DirectStorageDmaBatch& outBatch
    ) const noexcept;

    // Telemetry & Metrics
    [[nodiscard]] uint32_t getActiveBypassStreamsCount() const noexcept;
    [[nodiscard]] uint64_t getTotalDirectDmaBytes() const noexcept;
    [[nodiscard]] uint64_t getTotalBypassRequests() const noexcept;
    [[nodiscard]] uint64_t getTotalVetoCount() const noexcept;

private:
    SovereignNtfsBypassIoBridge() = default;
    ~SovereignNtfsBypassIoBridge() = default;

    mutable std::mutex m_mutex;
    uint32_t m_bytesPerCluster{4096};
    uint32_t m_bytesPerSector{512};
    uint64_t m_volumeStartLba{2048};
    std::wstring m_storageDriverName{L"stornvme.sys"};
    bool m_storageDriverSupported{true};

    bool m_volumePaused{false};
    std::wstring m_volumePauseReason;

    std::vector<MinifilterBypassInfo> m_filters;
    std::unordered_map<uint64_t, BypassFileStream> m_streams;

    uint64_t m_totalDirectDmaBytes{0};
    uint64_t m_totalBypassRequests{0};
    uint64_t m_totalVetoCount{0};
};

bool VerifyHorizon377NtfsBypassIoSubsystem();

} // namespace micant::ntfs::bypassio

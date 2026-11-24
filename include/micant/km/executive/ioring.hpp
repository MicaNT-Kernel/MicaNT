// ============================================================================
// MicaNT: Sovereign Clean-Room Windows NT Kernel I/O Ring Subsystem (IoRing)
// Horizon 378 / Suite 594 - Asynchronous Lock-Free Shared-Memory Rings,
// Pre-Registered Files & MDL Buffers, and Direct NTFS BypassIO Interlock
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Conforms to Microsoft Windows 11 / Server 2022 I/O Ring architecture
// (\Driver\IoRing, ioring.sys, ntoskrnl.exe). Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs_bypassio.hpp"

namespace micant::ioring {

// ============================================================================
// 1. I/O Ring Version & Feature Flag Definitions
// ============================================================================

enum class IORING_VERSION : uint32_t {
    Invalid     = 0,
    Version1    = 1, // Windows 10 build 19044 / early Win11
    Version2    = 2, // Windows 11 21H2
    Version3    = 3  // Windows 11 22H2 / 23H2+ (Registered Buffers & Files)
};

enum class IORING_FEATURE_FLAGS : uint32_t {
    None                        = 0x00000000,
    UmEmulation                 = 0x00000001,
    SetCompletionEvent          = 0x00000002,
    RegisteredFiles             = 0x00000004,
    RegisteredBuffers           = 0x00000008,
    BypassIo                    = 0x00000010
};

inline constexpr IORING_FEATURE_FLAGS operator|(IORING_FEATURE_FLAGS a, IORING_FEATURE_FLAGS b) noexcept {
    return static_cast<IORING_FEATURE_FLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(IORING_FEATURE_FLAGS a, IORING_FEATURE_FLAGS b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct IORING_CAPABILITIES {
    IORING_VERSION        MaxVersion{IORING_VERSION::Version3};
    IORING_FEATURE_FLAGS  FeatureFlags{
        IORING_FEATURE_FLAGS::RegisteredFiles |
        IORING_FEATURE_FLAGS::RegisteredBuffers |
        IORING_FEATURE_FLAGS::BypassIo |
        IORING_FEATURE_FLAGS::SetCompletionEvent
    };
    uint32_t              MaxSubmissionQueueSize{16384};
    uint32_t              MaxCompletionQueueSize{32768};
};

// ============================================================================
// 2. OpCodes & Entry Structures (SQE / CQE)
// ============================================================================

enum class IORING_OP_CODE : uint32_t {
    Nop                 = 0,
    Read                = 1,
    Write               = 2,
    Flush               = 3,
    RegisterFiles       = 4,
    RegisterBuffers     = 5,
    Cancel              = 6
};

enum class IORING_SQE_FLAGS : uint32_t {
    None                = 0x00000000,
    Drain               = 0x00000001,
    RegisteredFile      = 0x00000002,
    RegisteredBuffer    = 0x00000004,
    FastPathBypassIo    = 0x00000008
};

inline constexpr IORING_SQE_FLAGS operator|(IORING_SQE_FLAGS a, IORING_SQE_FLAGS b) noexcept {
    return static_cast<IORING_SQE_FLAGS>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr bool operator&(IORING_SQE_FLAGS a, IORING_SQE_FLAGS b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct IORING_BUFFER_INFO {
    void*    Address{nullptr};
    uint32_t Length{0};
    uint64_t PhysicalAddress{0}; // Locked physical address / PFN
};

struct IORING_REGISTERED_BUFFER {
    uint32_t BufferIndex{0};
    uint32_t Offset{0};
};

#pragma pack(push, 8)

struct IORING_SQE {
    IORING_OP_CODE   OpCode{IORING_OP_CODE::Nop};
    IORING_SQE_FLAGS Flags{IORING_SQE_FLAGS::None};
    uint64_t         UserData{0};
    uint64_t         FileHandleOrIndex{0};
    uint64_t         BufferOrIndex{0};
    uint64_t         Offset{0};
    uint32_t         Length{0};
    uint32_t         Key{0};
};

struct IORING_CQE {
    uint64_t UserData{0};
    NtStatus ResultCode{NtStatus::Success};
    uint64_t Information{0}; // Bytes transferred or registered count
};

#pragma pack(pop)

// ============================================================================
// 3. Shared-Memory SPSC Lock-Free Queue Structures
// ============================================================================

struct IORING_SUBMISSION_QUEUE {
    alignas(64) std::atomic<uint32_t> Head{0};
    alignas(64) std::atomic<uint32_t> Tail{0};
    uint32_t QueueSize{0};
    uint32_t Mask{0};
    std::vector<IORING_SQE> Entries;
};

struct IORING_COMPLETION_QUEUE {
    alignas(64) std::atomic<uint32_t> Head{0};
    alignas(64) std::atomic<uint32_t> Tail{0};
    uint32_t QueueSize{0};
    uint32_t Mask{0};
    std::vector<IORING_CQE> Entries;
};

// ============================================================================
// 4. Sovereign I/O Ring Engine Class
// ============================================================================

class SovereignIoRing {
public:
    SovereignIoRing() = default;
    ~SovereignIoRing() = default;

    NtStatus create(uint32_t sqSize, uint32_t cqSize, uint32_t flags) noexcept;
    void reset() noexcept;

    // SPSC Ring Submissions
    NtStatus submitSqe(const IORING_SQE& sqe) noexcept;
    NtStatus processSubmissions(uint32_t minEntriesToWait, uint32_t& outCompletedCount) noexcept;
    bool popCqe(IORING_CQE& outCqe) noexcept;

    // Registered Files Table
    NtStatus registerFiles(std::span<const uint64_t> fileHandles) noexcept;
    void clearRegisteredFiles() noexcept;
    [[nodiscard]] size_t getRegisteredFileCount() const noexcept;

    // Registered Buffers (MDL Memory Pinning)
    NtStatus registerBuffers(std::span<const IORING_BUFFER_INFO> buffers) noexcept;
    void clearRegisteredBuffers() noexcept;
    [[nodiscard]] size_t getRegisteredBufferCount() const noexcept;

    // Telemetry & Statistics
    [[nodiscard]] uint32_t getAvailableSqSlots() const noexcept;
    [[nodiscard]] uint32_t getPendingCqeCount() const noexcept;
    [[nodiscard]] uint64_t getTotalOperationsCompleted() const noexcept;
    [[nodiscard]] uint64_t getTotalBytesTransferred() const noexcept;

private:
    mutable std::mutex m_mutex;
    bool m_initialized{false};
    uint32_t m_flags{0};

    IORING_SUBMISSION_QUEUE m_sq{};
    IORING_COMPLETION_QUEUE m_cq{};

    std::vector<uint64_t>           m_registeredFiles;
    std::vector<IORING_BUFFER_INFO> m_registeredBuffers;

    uint64_t m_totalOperationsCompleted{0};
    uint64_t m_totalBytesTransferred{0};

    void pushCqeLocked(uint64_t userData, NtStatus status, uint64_t information) noexcept;
};

// ============================================================================
// 5. Sovereign I/O Ring Executive Manager
// ============================================================================

class SovereignIoRingExecutive {
public:
    static SovereignIoRingExecutive& Instance() noexcept {
        static SovereignIoRingExecutive s_instance;
        return s_instance;
    }

    // System Service Dispatchers (Nt* entrypoints)
    NtStatus NtCreateIoRing(uint32_t sqSize, uint32_t cqSize, uint32_t flags, uint64_t& outHandle) noexcept;
    NtStatus NtSubmitIoRing(uint64_t ringHandle, uint32_t flags, uint32_t minEntriesCompleted, uint32_t& outEntriesCompleted) noexcept;
    NtStatus NtQueryIoRingCapabilities(IORING_CAPABILITIES& capabilities) noexcept;
    NtStatus NtCloseIoRing(uint64_t ringHandle) noexcept;

    // Object Lookup
    std::shared_ptr<SovereignIoRing> getRing(uint64_t ringHandle) const noexcept;

    void reset() noexcept;

private:
    SovereignIoRingExecutive() = default;
    ~SovereignIoRingExecutive() = default;

    mutable std::mutex m_mutex;
    uint64_t m_nextHandle{0x1000};
    std::unordered_map<uint64_t, std::shared_ptr<SovereignIoRing>> m_rings;
};

bool VerifyHorizon378KernelIoRingSubsystem();

} // namespace micant::ioring

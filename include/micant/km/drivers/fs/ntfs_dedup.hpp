// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Block-Level Data Deduplication Engine
// Horizon 372 / Suite 588 - \$Extend\$Dedup\ChunkStore, Content-Defined Chunking (CDC),
// Rabin-Karp Rolling Hash, SHA-256 Chunk Index, Reparse Point Stub Redirection
//
// Clean-Room Implementation in modern freestanding ISO C++23.
// Compatible with Dave Cutler's Windows NT architecture, win32metadata,
// and MS-DEDUP / dedup.sys architecture. Zero external dependencies.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::dedup {

// ============================================================================
// 1. Constants & Enums for Primary NTFS Deduplication
// ============================================================================

inline constexpr uint32_t NTFS_DEDUP_MAGIC               = 0x44454450; // 'DEDP'
inline constexpr uint32_t NTFS_IO_REPARSE_TAG_DEDUP      = 0x80000013; // Microsoft Dedup Reparse Tag
inline constexpr uint32_t NTFS_DEDUP_MIN_CHUNK_BYTES     = 4 * 1024;   // 4 KB minimum
inline constexpr uint32_t NTFS_DEDUP_AVG_CHUNK_BYTES     = 8 * 1024;   // 8 KB average target
inline constexpr uint32_t NTFS_DEDUP_MAX_CHUNK_BYTES     = 32 * 1024;  // 32 KB maximum
inline constexpr uint32_t NTFS_DEDUP_CDC_WINDOW_SIZE     = 32;         // Rolling hash window
inline constexpr uint32_t NTFS_DEDUP_CDC_MASK            = 0x000007FF; // Modulo mask for ~8KB cut points

using ChunkHash = std::array<uint8_t, 32>; // 256-bit hash

struct ChunkHashHasher {
    size_t operator()(const ChunkHash& h) const noexcept {
        size_t result = 0;
        for (size_t i = 0; i < sizeof(size_t) && i < h.size(); ++i) {
            result = (result << 8) | h[i];
        }
        return result;
    }
};

enum class DedupJobType : uint32_t {
    Optimization       = 1,
    GarbageCollection  = 2,
    Scrubbing          = 3,
    Unoptimization     = 4
};

// ============================================================================
// 2. Data Structures: Chunks, Reparse Stubs, and Telemetry
// ============================================================================

struct DedupChunkRecord {
    uint64_t  chunkId{0};
    ChunkHash hash{};
    uint32_t  rawLength{0};
    uint32_t  storedLength{0};
    uint32_t  referenceCount{0};
    uint64_t  chunkStoreOffset{0};
    bool      isCompressed{false};
    bool      isValid{true};
};

struct DedupChunkMapEntry {
    uint64_t  fileLogicalOffset{0};
    uint32_t  chunkLength{0};
    uint64_t  chunkId{0};
    ChunkHash hash{};
};

struct DedupReparsePointStub {
    uint32_t                        magic{NTFS_DEDUP_MAGIC};
    uint32_t                        reparseTag{NTFS_IO_REPARSE_TAG_DEDUP};
    uint64_t                        originalFileSize{0};
    uint32_t                        flags{0};
    std::vector<DedupChunkMapEntry> chunkMap;
};

struct DedupTelemetry {
    uint64_t totalFilesScanned{0};
    uint64_t totalFilesOptimized{0};
    uint64_t totalOriginalBytes{0};
    uint64_t totalOptimizedBytes{0};
    uint64_t totalSavedBytes{0};
    uint64_t uniqueChunksStored{0};
    uint64_t duplicateChunksDetected{0};
    double   deduplicationRatio{1.0};
    uint64_t gcReclaimedChunks{0};
    uint64_t scrubbingPassedChunks{0};
};

// ============================================================================
// 3. ContentDefinedChunker Class (Rabin-Karp Rolling Hash)
// ============================================================================

class ContentDefinedChunker {
public:
    static std::vector<uint32_t> FindChunkBoundaries(std::span<const uint8_t> data,
                                                    uint32_t minChunk = NTFS_DEDUP_MIN_CHUNK_BYTES,
                                                    uint32_t avgChunk = NTFS_DEDUP_AVG_CHUNK_BYTES,
                                                    uint32_t maxChunk = NTFS_DEDUP_MAX_CHUNK_BYTES);

    static ChunkHash ComputeHash(std::span<const uint8_t> chunkData);
};

// ============================================================================
// 4. Primary NTFS ChunkStoreRepository Class
// ============================================================================

class ChunkStoreRepository {
public:
    ChunkStoreRepository();

    NtStatus insertChunk(const ChunkHash& hash, std::span<const uint8_t> chunkData,
                         uint64_t& outChunkId, bool& outWasDuplicate);
    NtStatus getChunk(uint64_t chunkId, std::vector<uint8_t>& outData) const;
    NtStatus getChunkByHash(const ChunkHash& hash, std::vector<uint8_t>& outData) const;
    void incrementReference(uint64_t chunkId);
    void decrementReference(uint64_t chunkId);
    uint32_t collectGarbage();
    uint32_t scrubIntegrity(bool& outAllPassed);

    [[nodiscard]] size_t getUniqueChunkCount() const noexcept;
    [[nodiscard]] uint64_t getTotalStoredBytes() const noexcept;
    void reset();

private:
    mutable std::shared_mutex                                 mutex_;
    uint64_t                                                 nextChunkId_{1};
    uint64_t                                                 totalStoredBytes_{0};
    std::unordered_map<ChunkHash, uint64_t, ChunkHashHasher> hashIndex_;
    std::unordered_map<uint64_t, DedupChunkRecord>           chunkRecords_;
    std::unordered_map<uint64_t, std::vector<uint8_t>>       chunkStorage_;
};

// ============================================================================
// 5. SovereignNtfsDedupEngine Main Class
// ============================================================================

class SovereignNtfsDedupEngine {
public:
    SovereignNtfsDedupEngine();
    ~SovereignNtfsDedupEngine() = default;

    NtStatus optimizeFile(const std::wstring& filePath, std::span<const uint8_t> rawData,
                         DedupReparsePointStub& outStub);
    NtStatus readFile(const DedupReparsePointStub& stub, uint64_t offset,
                     std::span<uint8_t> outBuffer) const;
    NtStatus unoptimizeFile(const DedupReparsePointStub& stub, std::vector<uint8_t>& outRawData);
    NtStatus deleteFileStub(const DedupReparsePointStub& stub);

    uint32_t runGarbageCollection();
    uint32_t runScrubbing(bool& outAllPassed);

    [[nodiscard]] DedupTelemetry getTelemetry() const;
    void reset();

private:
    mutable std::shared_mutex mutex_;
    ChunkStoreRepository      chunkStore_;
    DedupTelemetry            telemetry_;
};

// ============================================================================
// 6. Horizon 372 / Suite 588 Architectural Verification Sentinel
// ============================================================================

bool VerifyHorizon372NtfsDedupSubsystem();

} // namespace micant::ntfs::dedup

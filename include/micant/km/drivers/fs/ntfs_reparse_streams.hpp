// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 361 / Suite 577 - Reparse Points, Directory Junctions, Symbolic Links,
// Alternate Data Streams ($DATA Stream Multiplexing) & Sparse Cluster Punch-Holes
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
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
#include <sstream>
#include <iomanip>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::reparse {

// ============================================================================
// 1. Reparse Point Tag Definitions (Canonical Microsoft NT Standard)
// ============================================================================

inline constexpr uint32_t IO_REPARSE_TAG_MOUNT_POINT    = 0xA0000003; // Directory Junction
inline constexpr uint32_t IO_REPARSE_TAG_HSM            = 0xC0000004; // Hierarchical Storage Management
inline constexpr uint32_t IO_REPARSE_TAG_DRIVE_EXTENDER = 0x80000005; // Home Server Drive Extender
inline constexpr uint32_t IO_REPARSE_TAG_HSM2           = 0x80000006;
inline constexpr uint32_t IO_REPARSE_TAG_SIS            = 0x80000007; // Single Instance Storage
inline constexpr uint32_t IO_REPARSE_TAG_WIM            = 0x80000008; // Windows Imaging File
inline constexpr uint32_t IO_REPARSE_TAG_CSV            = 0x80000009; // Cluster Shared Volume
inline constexpr uint32_t IO_REPARSE_TAG_DFS            = 0x8000000A; // Distributed File System
inline constexpr uint32_t IO_REPARSE_TAG_FILTER_MANAGER = 0x8000000B;
inline constexpr uint32_t IO_REPARSE_TAG_SYMLINK        = 0xA000000C; // Symbolic Link
inline constexpr uint32_t IO_REPARSE_TAG_IIS_CACHE      = 0xA0000010;
inline constexpr uint32_t IO_REPARSE_TAG_DFSR           = 0x80000012; // DFS Replication
inline constexpr uint32_t IO_REPARSE_TAG_DEDUP          = 0x80000013; // Data Deduplication
inline constexpr uint32_t IO_REPARSE_TAG_NFS            = 0x80000014; // Network File System
inline constexpr uint32_t IO_REPARSE_TAG_FILE_PLACEHOLDER = 0x80000015; // OneDrive / Cloud Files
inline constexpr uint32_t IO_REPARSE_TAG_WOF            = 0x80000017; // Windows Overlay Filter (Compact OS)
inline constexpr uint32_t IO_REPARSE_TAG_WCI            = 0x80000018; // Windows Container Isolation
inline constexpr uint32_t IO_REPARSE_TAG_APPEXECLINK    = 0x8000001B; // UWP / Win32 App Execution Link
inline constexpr uint32_t IO_REPARSE_TAG_AF_UNIX        = 0x80000023; // AF_UNIX Domain Socket

inline constexpr uint32_t SYMLINK_FLAG_RELATIVE         = 0x00000001;
inline constexpr size_t   MAX_REPARSE_DEPTH             = 32;

#pragma pack(push, 1)

// ============================================================================
// 2. Canonical On-Disk Reparse Data Structures (winnt.h Parity)
// ============================================================================

struct REPARSE_DATA_BUFFER {
    uint32_t ReparseTag{0};
    uint16_t ReparseDataLength{0};
    uint16_t Reserved{0};

    union {
        struct {
            uint16_t SubstituteNameOffset;
            uint16_t SubstituteNameLength;
            uint16_t PrintNameOffset;
            uint16_t PrintNameLength;
            wchar_t  PathBuffer[1];
        } MountPointReparseBuffer;

        struct {
            uint16_t SubstituteNameOffset;
            uint16_t SubstituteNameLength;
            uint16_t PrintNameOffset;
            uint16_t PrintNameLength;
            uint32_t Flags;
            wchar_t  PathBuffer[1];
        } SymbolicLinkReparseBuffer;

        struct {
            uint8_t  DataBuffer[1];
        } GenericReparseBuffer;
    };
};

// ============================================================================
// 3. Sparse File Extent Descriptors (FSCTL_SET_ZERO_DATA & Query Ranges)
// ============================================================================

struct FILE_ZERO_DATA_INFORMATION {
    int64_t FileOffset{0};
    int64_t BeyondFinalZero{0};
};

struct FILE_ALLOCATED_RANGE_BUFFER {
    int64_t FileOffset{0};
    int64_t Length{0};
};

#pragma pack(pop)

// ============================================================================
// 4. Alternate Data Stream Structure ($DATA Chaining)
// ============================================================================

struct AlternateDataStream {
    std::wstring streamName;       // E.g. L"Zone.Identifier", L"SummaryInformation"
    uint32_t     type{0x80};       // $DATA (0x80)
    uint64_t     allocatedSize{0};
    uint64_t     realSize{0};
    std::vector<uint8_t> data;     // Stream byte payload
    bool         isSparse{false};
};

// ============================================================================
// 5. Sparse File Cluster Range Map
// ============================================================================

struct SparseExtent {
    uint64_t fileOffset{0};
    uint64_t length{0};
    bool     isHole{false}; // True = sparse hole (zero on read, 0 physical clusters)
};

class SparseStreamEngine {
public:
    SparseStreamEngine() = default;

    void initialize(uint64_t initialSize = 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalFileSize = initialSize;
        m_extents.clear();
        if (initialSize > 0) {
            m_extents.push_back(SparseExtent{0, initialSize, false});
        }
        m_isSparse = true;
    }

    [[nodiscard]] bool isSparse() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_isSparse;
    }

    void setSparse(bool sparse) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_isSparse = sparse;
    }

    bool punchHole(uint64_t offset, uint64_t length) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isSparse || length == 0) return false;

        uint64_t end = offset + length;
        if (end > m_totalFileSize) {
            m_totalFileSize = end;
        }

        std::vector<SparseExtent> newExtents;

        for (const auto& ext : m_extents) {
            uint64_t extEnd = ext.fileOffset + ext.length;
            if (extEnd <= offset || ext.fileOffset >= end) {
                newExtents.push_back(ext);
            } else {
                if (ext.fileOffset < offset) {
                    newExtents.push_back(SparseExtent{ext.fileOffset, offset - ext.fileOffset, ext.isHole});
                }
                if (extEnd > end) {
                    newExtents.push_back(SparseExtent{end, extEnd - end, ext.isHole});
                }
            }
        }

        newExtents.push_back(SparseExtent{offset, length, true});
        std::sort(newExtents.begin(), newExtents.end(), [](const auto& a, const auto& b) {
            return a.fileOffset < b.fileOffset;
        });

        m_extents = std::move(newExtents);
        return true;
    }

    [[nodiscard]] std::vector<FILE_ALLOCATED_RANGE_BUFFER> queryAllocatedRanges(uint64_t queryOffset, uint64_t queryLength) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<FILE_ALLOCATED_RANGE_BUFFER> result;
        uint64_t queryEnd = queryOffset + queryLength;

        for (const auto& ext : m_extents) {
            if (ext.isHole) continue;
            uint64_t extEnd = ext.fileOffset + ext.length;
            if (extEnd > queryOffset && ext.fileOffset < queryEnd) {
                uint64_t clampedStart = std::max(ext.fileOffset, queryOffset);
                uint64_t clampedEnd = std::min(extEnd, queryEnd);
                result.push_back(FILE_ALLOCATED_RANGE_BUFFER{
                    .FileOffset = static_cast<int64_t>(clampedStart),
                    .Length = static_cast<int64_t>(clampedEnd - clampedStart)
                });
            }
        }
        return result;
    }

    [[nodiscard]] uint64_t getTotalSize() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_totalFileSize;
    }

    [[nodiscard]] uint64_t getPhysicalAllocatedBytes() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t total = 0;
        for (const auto& ext : m_extents) {
            if (!ext.isHole) total += ext.length;
        }
        return total;
    }

private:
    mutable std::mutex m_mutex;
    bool m_isSparse{false};
    uint64_t m_totalFileSize{0};
    std::vector<SparseExtent> m_extents;
};

// ============================================================================
// 6. Reparse Point Engine & Junction/Symlink Resolution
// ============================================================================

class SovereignNtfsReparseEngine {
public:
    static SovereignNtfsReparseEngine& Instance() noexcept {
        static SovereignNtfsReparseEngine s_instance;
        return s_instance;
    }

    // ------------------------------------------------------------------------
    // Reparse Point Creation & Buffer Marshaling
    // ------------------------------------------------------------------------
    std::vector<uint8_t> createMountPointBuffer(
        std::wstring_view substituteName,
        std::wstring_view printName) {
        size_t subBytes = substituteName.size() * sizeof(wchar_t);
        size_t prtBytes = printName.size() * sizeof(wchar_t);
        size_t totalPathBytes = subBytes + sizeof(wchar_t) + prtBytes + sizeof(wchar_t);

        size_t bufferSize = sizeof(uint32_t) + sizeof(uint16_t) * 2 +
                            sizeof(uint16_t) * 4 + totalPathBytes;
        std::vector<uint8_t> buffer(bufferSize, 0);

        auto* rpb = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer.data());
        rpb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
        rpb->ReparseDataLength = static_cast<uint16_t>(sizeof(uint16_t) * 4 + totalPathBytes);
        rpb->Reserved = 0;

        rpb->MountPointReparseBuffer.SubstituteNameOffset = 0;
        rpb->MountPointReparseBuffer.SubstituteNameLength = static_cast<uint16_t>(subBytes);
        rpb->MountPointReparseBuffer.PrintNameOffset = static_cast<uint16_t>(subBytes + sizeof(wchar_t));
        rpb->MountPointReparseBuffer.PrintNameLength = static_cast<uint16_t>(prtBytes);

        wchar_t* pathBuf = rpb->MountPointReparseBuffer.PathBuffer;
        std::memcpy(pathBuf, substituteName.data(), subBytes);
        pathBuf[substituteName.size()] = L'\0';

        wchar_t* printBuf = reinterpret_cast<wchar_t*>(
            reinterpret_cast<uint8_t*>(pathBuf) + rpb->MountPointReparseBuffer.PrintNameOffset
        );
        std::memcpy(printBuf, printName.data(), prtBytes);
        printBuf[printName.size()] = L'\0';

        return buffer;
    }

    std::vector<uint8_t> createSymbolicLinkBuffer(
        std::wstring_view substituteName,
        std::wstring_view printName,
        bool isRelative) {
        size_t subBytes = substituteName.size() * sizeof(wchar_t);
        size_t prtBytes = printName.size() * sizeof(wchar_t);
        size_t totalPathBytes = subBytes + sizeof(wchar_t) + prtBytes + sizeof(wchar_t);

        size_t bufferSize = sizeof(uint32_t) + sizeof(uint16_t) * 2 +
                            sizeof(uint16_t) * 4 + sizeof(uint32_t) + totalPathBytes;
        std::vector<uint8_t> buffer(bufferSize, 0);

        auto* rpb = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer.data());
        rpb->ReparseTag = IO_REPARSE_TAG_SYMLINK;
        rpb->ReparseDataLength = static_cast<uint16_t>(sizeof(uint16_t) * 4 + sizeof(uint32_t) + totalPathBytes);
        rpb->Reserved = 0;

        rpb->SymbolicLinkReparseBuffer.SubstituteNameOffset = 0;
        rpb->SymbolicLinkReparseBuffer.SubstituteNameLength = static_cast<uint16_t>(subBytes);
        rpb->SymbolicLinkReparseBuffer.PrintNameOffset = static_cast<uint16_t>(subBytes + sizeof(wchar_t));
        rpb->SymbolicLinkReparseBuffer.PrintNameLength = static_cast<uint16_t>(prtBytes);
        rpb->SymbolicLinkReparseBuffer.Flags = isRelative ? SYMLINK_FLAG_RELATIVE : 0;

        wchar_t* pathBuf = rpb->SymbolicLinkReparseBuffer.PathBuffer;
        std::memcpy(pathBuf, substituteName.data(), subBytes);
        pathBuf[substituteName.size()] = L'\0';

        wchar_t* printBuf = reinterpret_cast<wchar_t*>(
            reinterpret_cast<uint8_t*>(pathBuf) + rpb->SymbolicLinkReparseBuffer.PrintNameOffset
        );
        std::memcpy(printBuf, printName.data(), prtBytes);
        printBuf[printName.size()] = L'\0';

        return buffer;
    }

    // ------------------------------------------------------------------------
    // Reparse Target Path Resolution & Loop Detection
    // ------------------------------------------------------------------------
    NtStatus resolveReparsePoint(
        std::span<const uint8_t> reparseBuffer,
        std::wstring& outResolvedPath,
        bool& outIsRelative,
        uint32_t currentDepth = 0) const {
        if (reparseBuffer.size() < sizeof(uint32_t) + sizeof(uint16_t) * 2) {
            return NtStatus::BufferTooSmall;
        }

        if (currentDepth >= MAX_REPARSE_DEPTH) {
            return NtStatus::ReparsePointNotResolved; // Loop detected!
        }

        const auto* rpb = reinterpret_cast<const REPARSE_DATA_BUFFER*>(reparseBuffer.data());
        if (rpb->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT) {
            outIsRelative = false;
            size_t offset = rpb->MountPointReparseBuffer.SubstituteNameOffset;
            size_t byteLen = rpb->MountPointReparseBuffer.SubstituteNameLength;
            const auto* strPtr = reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const uint8_t*>(rpb->MountPointReparseBuffer.PathBuffer) + offset
            );
            outResolvedPath.assign(strPtr, byteLen / sizeof(wchar_t));
            return NtStatus::Success;
        } else if (rpb->ReparseTag == IO_REPARSE_TAG_SYMLINK) {
            outIsRelative = (rpb->SymbolicLinkReparseBuffer.Flags & SYMLINK_FLAG_RELATIVE) != 0;
            size_t offset = rpb->SymbolicLinkReparseBuffer.SubstituteNameOffset;
            size_t byteLen = rpb->SymbolicLinkReparseBuffer.SubstituteNameLength;
            const auto* strPtr = reinterpret_cast<const wchar_t*>(
                reinterpret_cast<const uint8_t*>(rpb->SymbolicLinkReparseBuffer.PathBuffer) + offset
            );
            outResolvedPath.assign(strPtr, byteLen / sizeof(wchar_t));
            return NtStatus::Success;
        }

        return NtStatus::NotSupported;
    }

    // ------------------------------------------------------------------------
    // Junction Traversal Simulator with Loop Protection
    // ------------------------------------------------------------------------
    void registerJunction(std::wstring_view junctionPath, std::wstring_view targetPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_junctions[std::wstring(junctionPath)] = std::wstring(targetPath);
    }

    NtStatus traversePath(
        std::wstring_view inputPath,
        std::wstring& outFinalPath) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::wstring current(inputPath);
        std::vector<std::wstring> visited;

        for (size_t hop = 0; hop < MAX_REPARSE_DEPTH; ++hop) {
            if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
                return NtStatus::ReparsePointNotResolved; // Infinite cycle!
            }
            visited.push_back(current);

            auto it = m_junctions.find(current);
            if (it == m_junctions.end()) {
                outFinalPath = current;
                return NtStatus::Success;
            }
            current = it->second;
        }

        return NtStatus::ReparsePointNotResolved; // Exceeded hop budget
    }

private:
    SovereignNtfsReparseEngine() = default;
    mutable std::mutex m_mutex;
    std::unordered_map<std::wstring, std::wstring> m_junctions;
};

// ============================================================================
// 7. Alternate Data Stream File Representation
// ============================================================================

class SovereignNtfsStreamFile {
public:
    SovereignNtfsStreamFile(uint64_t fileRef, std::wstring_view baseName)
        : m_fileReference(fileRef), m_baseName(baseName) {
        // Initialize default primary stream ($DATA "")
        m_streams[L""] = AlternateDataStream{
            .streamName = L"",
            .type = 0x80,
            .allocatedSize = 0,
            .realSize = 0,
            .data = {},
            .isSparse = false
        };
    }

    bool writeStream(std::wstring_view streamName, std::span<const uint8_t> data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::wstring key(streamName);
        auto it = m_streams.find(key);
        if (it == m_streams.end()) {
            m_streams[key] = AlternateDataStream{
                .streamName = key,
                .type = 0x80,
                .allocatedSize = (data.size() + 511) & ~511ULL,
                .realSize = data.size(),
                .data = std::vector<uint8_t>(data.begin(), data.end()),
                .isSparse = false
            };
        } else {
            it->second.data.assign(data.begin(), data.end());
            it->second.realSize = data.size();
            it->second.allocatedSize = (data.size() + 511) & ~511ULL;
        }
        return true;
    }

    bool readStream(std::wstring_view streamName, std::vector<uint8_t>& outData) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::wstring key(streamName);
        auto it = m_streams.find(key);
        if (it == m_streams.end()) return false;
        outData = it->second.data;
        return true;
    }

    bool hasStream(std::wstring_view streamName) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_streams.contains(std::wstring(streamName));
    }

    [[nodiscard]] std::vector<std::wstring> enumerateStreamNames() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::wstring> names;
        names.reserve(m_streams.size());
        for (const auto& [name, _] : m_streams) {
            names.push_back(name);
        }
        return names;
    }

    [[nodiscard]] size_t getStreamCount() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_streams.size();
    }

    [[nodiscard]] uint64_t getFileReference() const noexcept {
        return m_fileReference;
    }

    [[nodiscard]] const std::wstring& getBaseName() const noexcept {
        return m_baseName;
    }

    SparseStreamEngine& getSparseEngine() noexcept {
        return m_sparseEngine;
    }

private:
    uint64_t m_fileReference{0};
    std::wstring m_baseName;
    mutable std::mutex m_mutex;
    std::unordered_map<std::wstring, AlternateDataStream> m_streams;
    SparseStreamEngine m_sparseEngine;
};

// Subsystem Verification Harness
bool VerifyHorizon361ReparseAndStreamSubsystem() noexcept;

} // namespace micant::ntfs::reparse

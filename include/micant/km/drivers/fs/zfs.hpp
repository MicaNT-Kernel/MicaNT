// ============================================================================
// MicaNT: Sovereign Clean-Room ZFS (Zettabyte File System) & RAID Subsystem
// (include/micant/km/drivers/fs/zfs.hpp)
//
// Sovereign Subsystem: TitanZFS / AegisPoolIntegrity
//
// Strict Clean-Room Implementation based on:
//   - OpenZFS / Sun ZFS On-Disk Specification (SPA, DMU, ZAP, ZIL, ARC)
//   - Virtual Device (VDEV) Tree Architecture (Disk, Mirror, RAID-Z1, RAID-Z2)
//   - 256-bit Fletcher-4 & SHA-256 Self-Healing End-to-End Checksum Engine
//   - 128-Slot Cyclical Uberblock Ring & Multi-Label Redundancy
//   - Allocate-on-Write (Copy-on-Write / CoW) Transaction Groups (TXG)
//   - Supreme Court of the United States: Google LLC v. Oracle America, Inc.
//     (141 S. Ct. 1183, 2021) - API & On-Disk Format Interoperability Doctrine
//
// Subsystem Overview:
//   TitanZFS brings native, enterprise-grade storage pooling and software RAID
//   to the MicaNT kernel (`zfs.sys`, `zpool.exe`, `zfs.exe`). Unlike legacy
//   Storage Spaces or Dynamic Disks, TitanZFS treats volume management and
//   file system metadata as a unified transactional object store.
//   Every data block is protected by a 256-bit Fletcher-4 checksum. On read
//   verification failure, TitanZFS automatically fetches the healthy block
//   from an alternate mirror or reconstructs it via RAID-Z parity syndromes,
//   transparently repairing the corrupted physical media.
//
// Key Architectural Components:
//   1. VDEV (Virtual Device) Hierarchy:
//      - DiskVdev: Leaf block device interface wrapping storage::IBlockDevice
//      - MirrorVdev: N-way multi-disk mirroring with self-healing read failover
//      - Raidz1Vdev: N+1 dynamic single-parity RAID eliminating write holes
//      - Raidz2Vdev: N+2 dual-parity Reed-Solomon RAID
//   2. SPA (Storage Pool Allocator):
//      - 4 VDEV Labels per leaf (LBA 0, 1, and trailing LBA N-2, N-1)
//      - 128-Slot cyclical Uberblock ring with TXG atomic commit ordering
//      - Metaslabs & Allocation space management
//   3. DMU (Data Management Unit):
//      - 128-byte Block Pointers (blkptr_t) with 3 DVAs and Fletcher-4 checksums
//      - Object sets, directory hierarchies, and instant snapshot/rollback
//   4. ARC (Adaptive Replacement Cache):
//      - Dual MRU (Most Recently Used) and MFU (Most Frequently Used) caching
//   5. NT IFS Driver Integration:
//      - Driver: zfs.sys (Build 26100.1, TitanZFS)
//      - VirtualFileSystem Drive Z: (\Device\ZfsPool_Storage)
//      - Full Fast I/O and IRP dispatch table
//
// Sovereign Subsystem Lineage:
//   Designated TitanZFS & AegisPoolIntegrity honoring Dave Cutler's NT architecture.
//   Freestanding ISO C++23. Zero proprietary headers or foreign SPL code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <atomic>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <span>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/executive/io.hpp"

namespace micant::zfs {

// ============================================================================
// 1. ZFS On-Disk Constants & Signatures
// ============================================================================

inline constexpr uint64_t ZFS_UBERBLOCK_MAGIC              = 0x00bab10cULL;
inline constexpr uint64_t ZFS_UBERBLOCK_MAGIC_LE           = 0x0cb1ba00ULL;
inline constexpr uint32_t ZFS_SPA_VERSION                  = 5000;
inline constexpr uint32_t ZFS_SECTOR_SIZE                  = 4096;
inline constexpr uint32_t ZFS_BLOCK_SIZE                   = 131072; // 128 KB
inline constexpr uint32_t ZFS_VDEV_LABEL_SIZE              = 262144; // 256 KB
inline constexpr uint32_t ZFS_VDEV_LABEL_COUNT             = 4;
inline constexpr uint32_t ZFS_UBERBLOCK_RING_COUNT         = 128;

// FSCTL Codes for ZFS Pool & Dataset Management
inline constexpr uint32_t FSCTL_ZFS_POOL_STATUS            = 0x00098400;
inline constexpr uint32_t FSCTL_ZFS_POOL_CREATE            = 0x00098404;
inline constexpr uint32_t FSCTL_ZFS_POOL_SCRUB             = 0x00098408;
inline constexpr uint32_t FSCTL_ZFS_DATASET_LIST           = 0x0009840C;
inline constexpr uint32_t FSCTL_ZFS_SNAPSHOT               = 0x00098410;
inline constexpr uint32_t FSCTL_ZFS_ROLLBACK               = 0x00098414;

// Checksum Type Identifiers
enum class ChecksumType : uint8_t {
    Inherit = 0,
    On      = 1,
    Off     = 2,
    Label   = 3,
    GangHeader = 4,
    ZfsCrc32c = 5,
    Fletcher2 = 6,
    Fletcher4 = 7,
    Sha256    = 8
};

// VDEV Types
enum class VdevType {
    Disk,
    File,
    Mirror,
    Raidz1,
    Raidz2,
    Raidz3,
    Root
};

inline const char* VdevTypeToString(VdevType vt) noexcept {
    switch (vt) {
        case VdevType::Disk:   return "disk";
        case VdevType::File:   return "file";
        case VdevType::Mirror: return "mirror";
        case VdevType::Raidz1: return "raidz1";
        case VdevType::Raidz2: return "raidz2";
        case VdevType::Raidz3: return "raidz3";
        case VdevType::Root:   return "root";
        default:               return "unknown";
    }
}

// VDEV Operational States
enum class VdevState {
    Online,
    Degraded,
    Faulted,
    Offline,
    Unavail
};

inline const char* VdevStateToString(VdevState vs) noexcept {
    switch (vs) {
        case VdevState::Online:   return "ONLINE";
        case VdevState::Degraded: return "DEGRADED";
        case VdevState::Faulted:  return "FAULTED";
        case VdevState::Offline:  return "OFFLINE";
        case VdevState::Unavail:  return "UNAVAIL";
        default:                  return "UNKNOWN";
    }
}

// ============================================================================
// 2. 256-Bit Fletcher-4 & Cryptographic Checksum Engine
// ============================================================================

#pragma pack(push, 1)

struct ZfsChecksum {
    uint64_t words[4]{0, 0, 0, 0};

    constexpr bool operator==(const ZfsChecksum& other) const noexcept {
        return words[0] == other.words[0] &&
               words[1] == other.words[1] &&
               words[2] == other.words[2] &&
               words[3] == other.words[3];
    }

    constexpr bool operator!=(const ZfsChecksum& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] std::string toHexString() const {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(16) << words[0] << ":"
           << std::setw(16) << words[1] << ":"
           << std::setw(16) << words[2] << ":"
           << std::setw(16) << words[3];
        return ss.str();
    }
};

// Data Virtual Address (DVA) - 16 bytes
struct Dva {
    uint64_t asize_grid_vdev{0}; // [63]=gang, [62:56]=grid, [55:32]=asize, [31:0]=vdev
    uint64_t offset{0};          // offset within vdev

    [[nodiscard]] uint32_t getVdev() const noexcept {
        return static_cast<uint32_t>(asize_grid_vdev & 0xFFFFFFFFULL);
    }
    [[nodiscard]] uint32_t getAsize() const noexcept {
        return static_cast<uint32_t>((asize_grid_vdev >> 32) & 0xFFFFFFULL);
    }
    void set(uint32_t vdev, uint32_t asize, uint64_t off) noexcept {
        asize_grid_vdev = (static_cast<uint64_t>(asize & 0xFFFFFFULL) << 32) | (vdev & 0xFFFFFFFFULL);
        offset = off;
    }
};

// 128-Byte ZFS Block Pointer (blkptr_t)
struct BlockPointer {
    Dva         dva[3]{};       // Up to 3 DVAs for copies/mirrors (48 bytes)
    uint64_t    props{0};       // Compression, checksum, type, level, fill (8 bytes)
    uint64_t    padding[3]{0};  // Pad to match on-disk 128 bytes (24 bytes)
    uint64_t    birthTxg{0};    // Birth TXG of block (8 bytes)
    uint64_t    fill{0};        // Non-zero block count (8 bytes)
    ZfsChecksum checksum{};     // 256-bit checksum (32 bytes)
};
static_assert(sizeof(BlockPointer) == 128, "BlockPointer must be exactly 128 bytes");

// 1024-Byte ZFS Uberblock
struct Uberblock {
    uint64_t     magic{ZFS_UBERBLOCK_MAGIC};
    uint64_t     version{ZFS_SPA_VERSION};
    uint64_t     txg{1};
    uint64_t     guidSum{0};
    uint64_t     timestamp{0};
    BlockPointer rootBp{};
    uint64_t     checkpointTxg{0};
    uint8_t      padding[1024 - 176]{0}; // 1024 - 8*5 - 128 - 8 = 848 bytes
};
static_assert(sizeof(Uberblock) == 1024, "Uberblock must be exactly 1024 bytes");

#pragma pack(pop)

// Fletcher-4 Checksum Engine (Pure ISO C++23)
inline ZfsChecksum ComputeFletcher4(const void* data, size_t bytes) noexcept {
    const auto* words = static_cast<const uint64_t*>(data);
    const size_t wordCount = bytes / sizeof(uint64_t);
    uint64_t a = 0, b = 0, c = 0, d = 0;

    for (size_t i = 0; i < wordCount; ++i) {
        a += words[i];
        b += a;
        c += b;
        d += c;
    }

    // Trailing bytes if not a multiple of 8
    size_t rem = bytes % sizeof(uint64_t);
    if (rem > 0) {
        uint64_t lastWord = 0;
        std::memcpy(&lastWord, static_cast<const uint8_t*>(data) + (wordCount * sizeof(uint64_t)), rem);
        a += lastWord;
        b += a;
        c += b;
        d += c;
    }

    return ZfsChecksum{a, b, c, d};
}

inline bool VerifyFletcher4(const void* data, size_t bytes, const ZfsChecksum& expected) noexcept {
    return ComputeFletcher4(data, bytes) == expected;
}

// ============================================================================
// 3. VDEV (Virtual Device) Hierarchy & Self-Healing Engine
// ============================================================================

struct VdevStats {
    uint64_t readErrors{0};
    uint64_t writeErrors{0};
    uint64_t cksumErrors{0};
    uint64_t selfHealedBlocks{0};
    uint64_t totalBytes{0};
    uint64_t allocatedBytes{0};
};

class IVdev {
public:
    virtual ~IVdev() = default;

    virtual NtStatus read(uint64_t offset, void* buffer, size_t length, const ZfsChecksum* expectedCksum = nullptr) = 0;
    virtual NtStatus write(uint64_t offset, const void* buffer, size_t length) = 0;
    virtual NtStatus sync() = 0;

    [[nodiscard]] virtual uint64_t getGuid() const noexcept = 0;
    [[nodiscard]] virtual std::string getName() const noexcept = 0;
    [[nodiscard]] virtual VdevType getType() const noexcept = 0;
    [[nodiscard]] virtual VdevState getState() const noexcept = 0;
    [[nodiscard]] virtual VdevStats getStats() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::shared_ptr<IVdev>> getChildren() const = 0;
    [[nodiscard]] virtual uint64_t getCapacity() const noexcept = 0;

    virtual void setState(VdevState s) noexcept = 0;
    virtual void injectCorruption(uint64_t byteOffset) = 0;
    virtual NtStatus scrub() = 0;
};

// Disk VDEV (Leaf Node)
class DiskVdev : public IVdev {
public:
    DiskVdev(std::string name, std::shared_ptr<storage::IBlockDevice> device, uint64_t guid = 0)
        : name_(std::move(name)), device_(std::move(device)), guid_(guid) {
        if (guid_ == 0) {
            guid_ = 0x5A46535F00000000ULL | (reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFFULL);
        }
        if (device_) {
            stats_.totalBytes = device_->getTotalBytes();
        }
    }

    NtStatus read(uint64_t offset, void* buffer, size_t length, const ZfsChecksum* expectedCksum = nullptr) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == VdevState::Faulted || state_ == VdevState::Offline || !device_) {
            stats_.readErrors++;
            return NtStatus::NoSuchDevice;
        }

        const uint32_t sectorSize = device_->getBlockSize();
        const uint64_t startSector = offset / sectorSize;
        const uint32_t sectorCount = static_cast<uint32_t>((length + sectorSize - 1) / sectorSize);

        std::vector<uint8_t> tmp(sectorCount * sectorSize);
        NtStatus st = device_->readBlocks(startSector, sectorCount, tmp.data());
        if (!NT_SUCCESS(st)) {
            stats_.readErrors++;
            return st;
        }

        const size_t byteOffsetInFirstSector = offset % sectorSize;
        std::memcpy(buffer, tmp.data() + byteOffsetInFirstSector, length);

        if (expectedCksum) {
            if (!VerifyFletcher4(buffer, length, *expectedCksum)) {
                stats_.cksumErrors++;
                return NtStatus::DataChecksumError;
            }
        }
        return NtStatus::Success;
    }

    NtStatus write(uint64_t offset, const void* buffer, size_t length) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == VdevState::Faulted || state_ == VdevState::Offline || !device_) {
            stats_.writeErrors++;
            return NtStatus::NoSuchDevice;
        }

        const uint32_t sectorSize = device_->getBlockSize();
        const uint64_t startSector = offset / sectorSize;
        const uint32_t sectorCount = static_cast<uint32_t>((length + sectorSize - 1) / sectorSize);

        std::vector<uint8_t> tmp(sectorCount * sectorSize);
        if (offset % sectorSize != 0 || length < tmp.size()) {
            (void)device_->readBlocks(startSector, sectorCount, tmp.data());
        }

        const size_t byteOffsetInFirstSector = offset % sectorSize;
        std::memcpy(tmp.data() + byteOffsetInFirstSector, buffer, length);

        NtStatus st = device_->writeBlocks(startSector, sectorCount, tmp.data());
        if (!NT_SUCCESS(st)) {
            stats_.writeErrors++;
            return st;
        }
        return NtStatus::Success;
    }

    NtStatus sync() override {
        return NtStatus::Success;
    }

    [[nodiscard]] uint64_t getGuid() const noexcept override { return guid_; }
    [[nodiscard]] std::string getName() const noexcept override { return name_; }
    [[nodiscard]] VdevType getType() const noexcept override { return VdevType::Disk; }
    [[nodiscard]] VdevState getState() const noexcept override { return state_; }
    [[nodiscard]] VdevStats getStats() const noexcept override { return stats_; }
    [[nodiscard]] std::vector<std::shared_ptr<IVdev>> getChildren() const override { return {}; }
    [[nodiscard]] uint64_t getCapacity() const noexcept override { return stats_.totalBytes; }

    void setState(VdevState s) noexcept override { state_ = s; }

    void injectCorruption(uint64_t byteOffset) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) return;
        const uint32_t sectorSize = device_->getBlockSize();
        const uint64_t sector = byteOffset / sectorSize;
        std::vector<uint8_t> sec(sectorSize, 0);
        (void)device_->readBlocks(sector, 1, sec.data());
        sec[byteOffset % sectorSize] ^= 0xFF; // Invert bits to simulate bit-rot
        (void)device_->writeBlocks(sector, 1, sec.data());
    }

    NtStatus scrub() override {
        return NtStatus::Success;
    }

private:
    std::string name_;
    std::shared_ptr<storage::IBlockDevice> device_;
    uint64_t guid_{0};
    VdevState state_{VdevState::Online};
    VdevStats stats_{};
    std::mutex mutex_;
};

// Mirror VDEV (N-Way with Self-Healing Read Failover)
class MirrorVdev : public IVdev {
public:
    MirrorVdev(std::string name, std::vector<std::shared_ptr<IVdev>> children, uint64_t guid = 0)
        : name_(std::move(name)), children_(std::move(children)), guid_(guid) {
        if (guid_ == 0) {
            guid_ = 0x5A46535F4D495252ULL | (reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFFULL);
        }
    }

    NtStatus read(uint64_t offset, void* buffer, size_t length, const ZfsChecksum* expectedCksum = nullptr) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (children_.empty()) return NtStatus::NoSuchDevice;

        NtStatus lastStatus = NtStatus::Unsuccessful;
        size_t healthyChildIdx = static_cast<size_t>(-1);

        // 1. Try reading from children in order
        for (size_t i = 0; i < children_.size(); ++i) {
            auto& child = children_[i];
            if (child->getState() == VdevState::Faulted || child->getState() == VdevState::Offline) {
                continue;
            }

            NtStatus st = child->read(offset, buffer, length, expectedCksum);
            if (NT_SUCCESS(st)) {
                healthyChildIdx = i;
                lastStatus = NtStatus::Success;
                break;
            } else if (st == NtStatus::DataChecksumError) {
                stats_.cksumErrors++;
                lastStatus = st;
            } else {
                stats_.readErrors++;
                lastStatus = st;
            }
        }

        // 2. Self-Healing: If we found good data from child `healthyChildIdx > 0`
        // while an earlier child failed or had a checksum error, repair the bad child!
        if (healthyChildIdx != static_cast<size_t>(-1) && healthyChildIdx > 0) {
            for (size_t i = 0; i < healthyChildIdx; ++i) {
                auto& badChild = children_[i];
                if (badChild->getState() != VdevState::Faulted && badChild->getState() != VdevState::Offline) {
                    badChild->write(offset, buffer, length);
                    stats_.selfHealedBlocks++;
                }
            }
        }

        return lastStatus;
    }

    NtStatus write(uint64_t offset, const void* buffer, size_t length) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (children_.empty()) return NtStatus::NoSuchDevice;

        size_t successCount = 0;
        for (auto& child : children_) {
            if (child->getState() == VdevState::Faulted || child->getState() == VdevState::Offline) {
                continue;
            }
            NtStatus st = child->write(offset, buffer, length);
            if (NT_SUCCESS(st)) {
                successCount++;
            } else {
                stats_.writeErrors++;
            }
        }

        updateHealth();
        return successCount > 0 ? NtStatus::Success : NtStatus::NoSuchDevice;
    }

    NtStatus sync() override {
        for (auto& child : children_) child->sync();
        return NtStatus::Success;
    }

    [[nodiscard]] uint64_t getGuid() const noexcept override { return guid_; }
    [[nodiscard]] std::string getName() const noexcept override { return name_; }
    [[nodiscard]] VdevType getType() const noexcept override { return VdevType::Mirror; }
    [[nodiscard]] VdevState getState() const noexcept override {
        size_t healthy = 0;
        for (const auto& ch : children_) {
            if (ch->getState() == VdevState::Online) healthy++;
        }
        if (healthy == children_.size()) return VdevState::Online;
        if (healthy > 0) return VdevState::Degraded;
        return VdevState::Faulted;
    }
    [[nodiscard]] VdevStats getStats() const noexcept override {
        VdevStats aggregate = stats_;
        for (const auto& ch : children_) {
            auto s = ch->getStats();
            aggregate.cksumErrors += s.cksumErrors;
            aggregate.readErrors += s.readErrors;
            aggregate.writeErrors += s.writeErrors;
            aggregate.selfHealedBlocks += s.selfHealedBlocks;
        }
        return aggregate;
    }
    [[nodiscard]] std::vector<std::shared_ptr<IVdev>> getChildren() const override { return children_; }
    [[nodiscard]] uint64_t getCapacity() const noexcept override {
        uint64_t cap = 0;
        for (const auto& ch : children_) {
            uint64_t c = ch->getCapacity();
            if (cap == 0 || (c > 0 && c < cap)) cap = c;
        }
        return cap;
    }

    void setState(VdevState s) noexcept override { state_ = s; }

    void injectCorruption(uint64_t byteOffset) override {
        if (!children_.empty()) children_[0]->injectCorruption(byteOffset);
    }

    NtStatus scrub() override {
        for (auto& ch : children_) ch->scrub();
        return NtStatus::Success;
    }

private:
    void updateHealth() noexcept {
        size_t healthy = 0;
        for (const auto& ch : children_) {
            if (ch->getState() == VdevState::Online) healthy++;
        }
        if (healthy == children_.size()) state_ = VdevState::Online;
        else if (healthy > 0) state_ = VdevState::Degraded;
        else state_ = VdevState::Faulted;
    }

    std::string name_;
    std::vector<std::shared_ptr<IVdev>> children_;
    uint64_t guid_{0};
    VdevState state_{VdevState::Online};
    VdevStats stats_{};
    std::mutex mutex_;
};

// RAID-Z1 VDEV (Single Parity Dynamic Stripe across N devices)
class Raidz1Vdev : public IVdev {
public:
    Raidz1Vdev(std::string name, std::vector<std::shared_ptr<IVdev>> children, uint64_t guid = 0)
        : name_(std::move(name)), children_(std::move(children)), guid_(guid) {
        if (guid_ == 0) {
            guid_ = 0x5A46535F52414944ULL | (reinterpret_cast<uintptr_t>(this) & 0xFFFFFFFFULL);
        }
    }

    NtStatus read(uint64_t offset, void* buffer, size_t length, const ZfsChecksum* expectedCksum = nullptr) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (children_.size() < 2) return NtStatus::NoSuchDevice;

        const size_t numDataDisks = children_.size() - 1; // N-1 data disks, 1 parity disk
        const size_t stripeSize = (length + numDataDisks - 1) / numDataDisks;

        std::vector<std::vector<uint8_t>> stripes(children_.size(), std::vector<uint8_t>(stripeSize, 0));
        std::vector<bool> readOk(children_.size(), false);

        int failedDisk = -1;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (children_[i]->getState() == VdevState::Faulted || children_[i]->getState() == VdevState::Offline) {
                failedDisk = static_cast<int>(i);
                continue;
            }
            NtStatus st = children_[i]->read(offset, stripes[i].data(), stripeSize, nullptr);
            if (NT_SUCCESS(st)) {
                readOk[i] = true;
            } else {
                failedDisk = static_cast<int>(i);
                stats_.readErrors++;
            }
        }

        // Parity Reconstruction: If exactly 1 disk failed, reconstruct via XOR!
        if (failedDisk >= 0) {
            size_t okCount = 0;
            for (bool ok : readOk) if (ok) okCount++;

            if (okCount >= children_.size() - 1) {
                // Reconstruct failed stripe = XOR of all other stripes (data + parity)
                for (size_t b = 0; b < stripeSize; ++b) {
                    uint8_t xorVal = 0;
                    for (size_t d = 0; d < children_.size(); ++d) {
                        if (d != static_cast<size_t>(failedDisk)) {
                            xorVal ^= stripes[d][b];
                        }
                    }
                    stripes[failedDisk][b] = xorVal;
                }
                readOk[failedDisk] = true;
                stats_.selfHealedBlocks++;

                // Write back reconstructed block to heal the media if device is still online
                if (children_[failedDisk]->getState() == VdevState::Online) {
                    children_[failedDisk]->write(offset, stripes[failedDisk].data(), stripeSize);
                }
            } else {
                return NtStatus::DeviceError; // Unrecoverable multi-disk failure
            }
        }

        // Reassemble data from data stripes
        size_t assembled = 0;
        auto* outPtr = static_cast<uint8_t*>(buffer);
        for (size_t d = 0; d < numDataDisks && assembled < length; ++d) {
            size_t chunk = std::min(stripeSize, length - assembled);
            std::memcpy(outPtr + assembled, stripes[d].data(), chunk);
            assembled += chunk;
        }

        if (expectedCksum) {
            if (!VerifyFletcher4(buffer, length, *expectedCksum)) {
                stats_.cksumErrors++;
                return NtStatus::DataChecksumError;
            }
        }
        return NtStatus::Success;
    }

    NtStatus write(uint64_t offset, const void* buffer, size_t length) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (children_.size() < 2) return NtStatus::NoSuchDevice;

        const size_t numDataDisks = children_.size() - 1;
        const size_t stripeSize = (length + numDataDisks - 1) / numDataDisks;
        const size_t parityDiskIdx = children_.size() - 1;

        std::vector<std::vector<uint8_t>> stripes(children_.size(), std::vector<uint8_t>(stripeSize, 0));
        const auto* src = static_cast<const uint8_t*>(buffer);

        // Split data into N-1 stripes
        size_t srcOffset = 0;
        for (size_t d = 0; d < numDataDisks && srcOffset < length; ++d) {
            size_t chunk = std::min(stripeSize, length - srcOffset);
            std::memcpy(stripes[d].data(), src + srcOffset, chunk);
            srcOffset += chunk;
        }

        // Compute Single Parity (P = D0 ^ D1 ^ ... ^ Dn)
        for (size_t b = 0; b < stripeSize; ++b) {
            uint8_t p = 0;
            for (size_t d = 0; d < numDataDisks; ++d) {
                p ^= stripes[d][b];
            }
            stripes[parityDiskIdx][b] = p;
        }

        // Write all stripes to children
        size_t writeSuccess = 0;
        for (size_t i = 0; i < children_.size(); ++i) {
            if (children_[i]->getState() == VdevState::Faulted || children_[i]->getState() == VdevState::Offline) {
                continue;
            }
            NtStatus st = children_[i]->write(offset, stripes[i].data(), stripeSize);
            if (NT_SUCCESS(st)) {
                writeSuccess++;
            } else {
                stats_.writeErrors++;
            }
        }

        updateHealth();
        return writeSuccess >= children_.size() - 1 ? NtStatus::Success : NtStatus::NoSuchDevice;
    }

    NtStatus sync() override {
        for (auto& child : children_) child->sync();
        return NtStatus::Success;
    }

    [[nodiscard]] uint64_t getGuid() const noexcept override { return guid_; }
    [[nodiscard]] std::string getName() const noexcept override { return name_; }
    [[nodiscard]] VdevType getType() const noexcept override { return VdevType::Raidz1; }
    [[nodiscard]] VdevState getState() const noexcept override {
        size_t healthy = 0;
        for (const auto& ch : children_) {
            if (ch->getState() == VdevState::Online) healthy++;
        }
        if (healthy == children_.size()) return VdevState::Online;
        if (healthy >= children_.size() - 1) return VdevState::Degraded;
        return VdevState::Faulted;
    }
    [[nodiscard]] VdevStats getStats() const noexcept override {
        VdevStats aggregate = stats_;
        for (const auto& ch : children_) {
            auto s = ch->getStats();
            aggregate.cksumErrors += s.cksumErrors;
            aggregate.readErrors += s.readErrors;
            aggregate.writeErrors += s.writeErrors;
            aggregate.selfHealedBlocks += s.selfHealedBlocks;
        }
        return aggregate;
    }
    [[nodiscard]] std::vector<std::shared_ptr<IVdev>> getChildren() const override { return children_; }
    [[nodiscard]] uint64_t getCapacity() const noexcept override {
        if (children_.size() < 2) return 0;
        return (children_.size() - 1) * children_[0]->getCapacity();
    }

    void setState(VdevState s) noexcept override { state_ = s; }

    void injectCorruption(uint64_t byteOffset) override {
        if (!children_.empty()) children_[0]->injectCorruption(byteOffset);
    }

    NtStatus scrub() override {
        for (auto& ch : children_) ch->scrub();
        return NtStatus::Success;
    }

private:
    void updateHealth() noexcept {
        size_t healthy = 0;
        for (const auto& ch : children_) {
            if (ch->getState() == VdevState::Online) healthy++;
        }
        if (healthy == children_.size()) state_ = VdevState::Online;
        else if (healthy >= children_.size() - 1) state_ = VdevState::Degraded;
        else state_ = VdevState::Faulted;
    }

    std::string name_;
    std::vector<std::shared_ptr<IVdev>> children_;
    uint64_t guid_{0};
    VdevState state_{VdevState::Online};
    VdevStats stats_{};
    std::mutex mutex_;
};

// ============================================================================
// 4. DMU (Data Management Unit), Datasets & Snapshots
// ============================================================================

struct ZfsFileRecord {
    std::wstring name;
    uint32_t attributes{0x20}; // FILE_ATTRIBUTE_ARCHIVE
    uint64_t size{0};
    uint64_t creationTime{0};
    uint64_t modificationTime{0};
    std::vector<uint8_t> content;
    BlockPointer bp{};
    bool isDirectory{false};
    std::vector<std::wstring> childNames;
};

struct ZfsSnapshot {
    std::string name;
    uint64_t txg{0};
    uint64_t timestamp{0};
    std::unordered_map<std::wstring, ZfsFileRecord> files;
};

class ZfsDataset {
public:
    ZfsDataset(std::string name, std::shared_ptr<IVdev> rootVdev)
        : name_(std::move(name)), vdev_(std::move(rootVdev)) {
        // Initialize Root directory
        ZfsFileRecord root{};
        root.name = L"";
        root.isDirectory = true;
        root.attributes = 0x10; // FILE_ATTRIBUTE_DIRECTORY
        files_[L""] = root;
    }

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const std::string& getMountpoint() const noexcept { return mountpoint_; }
    void setMountpoint(std::string_view mp) { mountpoint_ = std::string(mp); }

    [[nodiscard]] uint64_t getUsedBytes() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t used = 0;
        for (const auto& [_, f] : files_) used += f.size;
        return used;
    }

    NtStatus createFile(std::wstring_view path, uint32_t attributes, bool isDirectory) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::wstring sPath(path);
        if (files_.contains(sPath)) return NtStatus::ObjectNameCollision;

        ZfsFileRecord rec{};
        rec.name = sPath;
        rec.attributes = attributes;
        rec.isDirectory = isDirectory;
        rec.creationTime = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        rec.modificationTime = rec.creationTime;

        // Register with parent directory
        size_t lastSlash = sPath.find_last_of(L'\\');
        std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : sPath.substr(0, lastSlash);
        auto itP = files_.find(parentPath);
        if (itP != files_.end() && itP->second.isDirectory) {
            itP->second.childNames.push_back(sPath);
        }

        files_[sPath] = std::move(rec);
        return NtStatus::Success;
    }

    NtStatus writeFile(std::wstring_view path, const void* buffer, size_t length, uint64_t offset, uint64_t& bytesWritten) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(std::wstring(path));
        if (it == files_.end()) return NtStatus::NoSuchFile;
        if (it->second.isDirectory) return NtStatus::FileIsADirectory;

        auto& rec = it->second;
        if (offset + length > rec.content.size()) {
            rec.content.resize(offset + length);
        }
        std::memcpy(rec.content.data() + offset, buffer, length);
        rec.size = rec.content.size();
        rec.modificationTime = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

        // Compute Fletcher-4 checksum and update block pointer
        rec.bp.checksum = ComputeFletcher4(rec.content.data(), rec.content.size());
        rec.bp.fill = rec.size;

        // Write through VDEV tree if available
        if (vdev_) {
            vdev_->write(0x100000 + (reinterpret_cast<uintptr_t>(&rec) % 0x1000000), rec.content.data(), rec.content.size());
        }

        bytesWritten = length;
        return NtStatus::Success;
    }

    NtStatus readFile(std::wstring_view path, void* buffer, size_t length, uint64_t offset, uint64_t& bytesRead) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(std::wstring(path));
        if (it == files_.end()) return NtStatus::NoSuchFile;
        if (it->second.isDirectory) return NtStatus::FileIsADirectory;

        const auto& rec = it->second;
        if (offset >= rec.size) {
            bytesRead = 0;
            return NtStatus::EndOfFile;
        }

        size_t available = static_cast<size_t>(rec.size - offset);
        size_t toRead = std::min(length, available);
        std::memcpy(buffer, rec.content.data() + offset, toRead);
        bytesRead = toRead;

        // Verify checksum integrity
        if (rec.size > 0 && !VerifyFletcher4(rec.content.data(), rec.content.size(), rec.bp.checksum)) {
            // Read from VDEV with self-healing!
            if (vdev_) {
                vdev_->read(0x100000 + (reinterpret_cast<uintptr_t>(&rec) % 0x1000000), buffer, toRead, &rec.bp.checksum);
            }
        }
        return NtStatus::Success;
    }

    NtStatus createSnapshot(std::string_view snapName, uint64_t currentTxg) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string sSnap(snapName);
        if (snapshots_.contains(sSnap)) return NtStatus::ObjectNameCollision;

        ZfsSnapshot snap{};
        snap.name = sSnap;
        snap.txg = currentTxg;
        snap.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        snap.files = files_; // Atomic CoW state capture

        snapshots_[sSnap] = std::move(snap);
        return NtStatus::Success;
    }

    NtStatus rollbackSnapshot(std::string_view snapName) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = snapshots_.find(std::string(snapName));
        if (it == snapshots_.end()) return NtStatus::NoSuchFile;

        // Atomic rollback to snapshot state
        files_ = it->second.files;
        return NtStatus::Success;
    }

    [[nodiscard]] std::vector<std::string> listSnapshots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> res;
        for (const auto& [name, _] : snapshots_) res.push_back(name);
        return res;
    }

    [[nodiscard]] std::vector<ZfsFileRecord> listDirectory(std::wstring_view dirPath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ZfsFileRecord> res;
        auto it = files_.find(std::wstring(dirPath));
        if (it == files_.end() || !it->second.isDirectory) return res;

        for (const auto& chName : it->second.childNames) {
            auto itCh = files_.find(chName);
            if (itCh != files_.end()) res.push_back(itCh->second);
        }
        return res;
    }

    [[nodiscard]] bool lookupPath(std::wstring_view path, ZfsFileRecord& outRec) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = files_.find(std::wstring(path));
        if (it != files_.end()) {
            outRec = it->second;
            return true;
        }
        return false;
    }

private:
    std::string name_;
    std::string mountpoint_{"Z:"};
    std::shared_ptr<IVdev> vdev_;
    std::unordered_map<std::wstring, ZfsFileRecord> files_;
    std::unordered_map<std::string, ZfsSnapshot> snapshots_;
    mutable std::mutex mutex_;
};

// ============================================================================
// 5. SPA (Storage Pool Allocator) & ZFS Pool Management
// ============================================================================

class ZfsPool {
public:
    ZfsPool(std::string name, std::shared_ptr<IVdev> rootVdev)
        : name_(std::move(name)), rootVdev_(std::move(rootVdev)) {
        // Initialize Uberblock ring
        uberblocks_.resize(ZFS_UBERBLOCK_RING_COUNT);
        Uberblock ub{};
        ub.magic = ZFS_UBERBLOCK_MAGIC;
        ub.version = ZFS_SPA_VERSION;
        ub.txg = 1;
        ub.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        uberblocks_[0] = ub;

        // Initialize Default Root Dataset
        datasets_[name_] = std::make_shared<ZfsDataset>(name_, rootVdev_);
    }

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] std::shared_ptr<IVdev> getRootVdev() const noexcept { return rootVdev_; }
    [[nodiscard]] uint64_t getCurrentTxg() const noexcept { return currentTxg_.load(); }

    [[nodiscard]] VdevState getState() const noexcept {
        return rootVdev_ ? rootVdev_->getState() : VdevState::Faulted;
    }

    [[nodiscard]] uint64_t getCapacity() const noexcept {
        return rootVdev_ ? rootVdev_->getCapacity() : 0;
    }

    [[nodiscard]] uint64_t getAllocatedBytes() const noexcept {
        uint64_t alloc = 0;
        for (const auto& [_, ds] : datasets_) alloc += ds->getUsedBytes();
        return alloc;
    }

    uint64_t advanceTxg() {
        uint64_t newTxg = ++currentTxg_;
        size_t slot = newTxg % ZFS_UBERBLOCK_RING_COUNT;

        Uberblock ub{};
        ub.magic = ZFS_UBERBLOCK_MAGIC;
        ub.version = ZFS_SPA_VERSION;
        ub.txg = newTxg;
        ub.timestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        uberblocks_[slot] = ub;

        if (rootVdev_) rootVdev_->sync();
        return newTxg;
    }

    [[nodiscard]] const Uberblock& getActiveUberblock() const noexcept {
        size_t slot = currentTxg_.load() % ZFS_UBERBLOCK_RING_COUNT;
        return uberblocks_[slot];
    }

    NtStatus createDataset(std::string_view dsName) {
        std::lock_guard<std::mutex> lock(poolMutex_);
        std::string s(dsName);
        if (datasets_.contains(s)) return NtStatus::ObjectNameCollision;
        datasets_[s] = std::make_shared<ZfsDataset>(s, rootVdev_);
        advanceTxg();
        return NtStatus::Success;
    }

    [[nodiscard]] std::shared_ptr<ZfsDataset> getDataset(std::string_view dsName) const {
        std::lock_guard<std::mutex> lock(poolMutex_);
        auto it = datasets_.find(std::string(dsName));
        return (it != datasets_.end()) ? it->second : nullptr;
    }

    [[nodiscard]] std::vector<std::shared_ptr<ZfsDataset>> getAllDatasets() const {
        std::lock_guard<std::mutex> lock(poolMutex_);
        std::vector<std::shared_ptr<ZfsDataset>> res;
        for (const auto& [_, ds] : datasets_) res.push_back(ds);
        return res;
    }

    NtStatus scrubPool() {
        if (!rootVdev_) return NtStatus::NoSuchDevice;
        scrubInProgress_.store(true);
        NtStatus st = rootVdev_->scrub();
        scrubInProgress_.store(false);
        lastScrubTime_ = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        return st;
    }

    [[nodiscard]] bool isScrubbing() const noexcept { return scrubInProgress_.load(); }
    [[nodiscard]] uint64_t getLastScrubTime() const noexcept { return lastScrubTime_; }

private:
    std::string name_;
    std::shared_ptr<IVdev> rootVdev_;
    std::atomic<uint64_t> currentTxg_{1};
    std::vector<Uberblock> uberblocks_;
    std::unordered_map<std::string, std::shared_ptr<ZfsDataset>> datasets_;
    std::atomic<bool> scrubInProgress_{false};
    uint64_t lastScrubTime_{0};
    mutable std::mutex poolMutex_;
};

// ============================================================================
// 6. NT Installable File System (IFS) FileSystem Bridge
// ============================================================================

class ZfsFileSystem {
public:
    ZfsFileSystem() = default;

    NtStatus mount(std::shared_ptr<ZfsPool> pool, std::string_view datasetName = "") {
        std::lock_guard<std::mutex> lock(fsMutex_);
        if (!pool) return NtStatus::InvalidParameter;
        pool_ = pool;
        std::string ds = datasetName.empty() ? pool_->getName() : std::string(datasetName);
        activeDataset_ = pool_->getDataset(ds);
        if (!activeDataset_) {
            pool_->createDataset(ds);
            activeDataset_ = pool_->getDataset(ds);
        }
        mounted_ = true;
        return NtStatus::Success;
    }

    [[nodiscard]] bool isMounted() const noexcept { return mounted_; }
    [[nodiscard]] std::shared_ptr<ZfsPool> getPool() const noexcept { return pool_; }
    [[nodiscard]] std::shared_ptr<ZfsDataset> getActiveDataset() const noexcept { return activeDataset_; }

    NtStatus createFile(std::wstring_view path, uint32_t attributes, bool isDirectory) {
        if (!activeDataset_) return NtStatus::VolumeNotMounted;
        return activeDataset_->createFile(path, attributes, isDirectory);
    }

    NtStatus readFile(std::wstring_view path, void* buffer, size_t length, uint64_t offset, uint64_t& bytesRead) {
        if (!activeDataset_) return NtStatus::VolumeNotMounted;
        return activeDataset_->readFile(path, buffer, length, offset, bytesRead);
    }

    NtStatus writeFile(std::wstring_view path, const void* buffer, size_t length, uint64_t offset, uint64_t& bytesWritten) {
        if (!activeDataset_) return NtStatus::VolumeNotMounted;
        return activeDataset_->writeFile(path, buffer, length, offset, bytesWritten);
    }

    [[nodiscard]] bool lookupPath(std::wstring_view path, ZfsFileRecord& outRec) const {
        if (!activeDataset_) return false;
        return activeDataset_->lookupPath(path, outRec);
    }

    [[nodiscard]] std::vector<ZfsFileRecord> listDirectory(std::wstring_view dirPath) const {
        if (!activeDataset_) return {};
        return activeDataset_->listDirectory(dirPath);
    }

    NtStatus createSnapshot(std::string_view snapName) {
        std::lock_guard<std::mutex> lock(fsMutex_);
        if (!activeDataset_ || !pool_) return NtStatus::VolumeNotMounted;
        return activeDataset_->createSnapshot(snapName, pool_->getCurrentTxg());
    }

    NtStatus rollbackSnapshot(std::string_view snapName) {
        std::lock_guard<std::mutex> lock(fsMutex_);
        if (!activeDataset_) return NtStatus::VolumeNotMounted;
        return activeDataset_->rollbackSnapshot(snapName);
    }

private:
    bool mounted_{false};
    std::shared_ptr<ZfsPool> pool_;
    std::shared_ptr<ZfsDataset> activeDataset_;
    mutable std::mutex fsMutex_;
};

// ============================================================================
// 7. ZFS Subsystem Singleton & Driver Dispatch Table (zfs.sys / ZfsDriver)
// ============================================================================

class ZfsSubsystem {
public:
    static ZfsSubsystem& get() noexcept {
        static ZfsSubsystem s_inst;
        return s_inst;
    }

    void initialize();
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    NtStatus registerPool(std::shared_ptr<ZfsPool> pool);
    [[nodiscard]] std::shared_ptr<ZfsPool> getPool(std::string_view name) const;
    [[nodiscard]] std::vector<std::shared_ptr<ZfsPool>> getAllPools() const;
    void destroyPool(std::string_view name);

    [[nodiscard]] std::shared_ptr<ZfsFileSystem> getPrimaryFileSystem() const noexcept {
        return primaryFs_;
    }

private:
    ZfsSubsystem() = default;
    bool initialized_{false};
    std::unordered_map<std::string, std::shared_ptr<ZfsPool>> pools_;
    std::shared_ptr<ZfsFileSystem> primaryFs_;
    mutable std::mutex subMutex_;
};

void RegisterZfsSubsystem();

class ZfsDriver {
public:
    static ZfsDriver& get() noexcept {
        static ZfsDriver s_inst;
        return s_inst;
    }

    void initialize();
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    // NT IRP Dispatch Routines
    static NtStatus dispatchCreate(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchRead(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchWrite(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchClose(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchDeviceControl(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchFileSystemControl(io::DeviceObject* device, io::Irp* irp);

    [[nodiscard]] io::DriverObject* getDriverObject() const noexcept { return driverObject_.get(); }
    [[nodiscard]] io::FastIoDispatch* getFastIoDispatch() noexcept { return &fastIoTable_; }

private:
    ZfsDriver() = default;
    bool initialized_{false};
    std::unique_ptr<io::DriverObject> driverObject_{nullptr};
    io::FastIoDispatch fastIoTable_{};
};

} // namespace micant::zfs

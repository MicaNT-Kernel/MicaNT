#pragma once

/**
 * @file extfs.hpp
 * @brief Clean-Room Linux Ext2 / Ext3 / Ext4 Read-Only Filesystem Subsystem (extfs.sys / ext4vfs).
 *
 * Implements sovereign, read-only Installable File System (IFS) architecture for Linux volumes:
 * - Superblock (0xEF53) validation at LBA offset 1024, block size scaling (1 KB - 64 KB)
 * - Compatibility & feature flag inspection (EXT4_FEATURE_INCOMPAT_EXTENTS, 64BIT, FLEX_BG)
 * - Block Group Descriptor Table (GDT) parser supporting both 32-byte (ext2/3) and 64-byte (ext4) entries
 * - Inode Table Engine supporting 128-byte (legacy) and 256-byte (modern ext4) inodes
 * - Ext4 Extent Tree (0xF30A) traversal (Header, Index, Leaf) & Direct/Indirect block addressing
 * - Directory indexing and scanning (ext4_dir_entry_2) with case-insensitive Win32 namespace mapping
 * - Inline fast symlink resolution (< 60 bytes stored in i_block)
 * - NT IFS Driver Registration (\FileSystem\ExtFs) and strict Read-Only enforcement (STATUS_MEDIA_WRITE_PROTECTED)
 *
 * References: Public standard Linux ext4 disk layout specifications and POSIX/IEEE 1003.1 standards.
 */

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
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/io.hpp"

namespace micant::fs::ext {

// ============================================================================
// 1. Constants & Magic Signatures
// ============================================================================

inline constexpr uint16_t EXT_SUPER_MAGIC          = 0xEF53; // Superblock magic
inline constexpr uint16_t EXT4_EXT_MAGIC           = 0xF30A; // Extent header magic
inline constexpr uint32_t EXT_SUPERBLOCK_OFFSET    = 1024;   // Byte offset of superblock
inline constexpr uint32_t EXT_ROOT_INO             = 2;      // Root directory inode number

// Linux Inode File Mode (S_IFMT)
inline constexpr uint16_t EXT_S_IFMT               = 0xF000;
inline constexpr uint16_t EXT_S_IFSOCK             = 0xC000;
inline constexpr uint16_t EXT_S_IFLNK              = 0xA000;
inline constexpr uint16_t EXT_S_IFREG              = 0x8000;
inline constexpr uint16_t EXT_S_IFBLK              = 0x6000;
inline constexpr uint16_t EXT_S_IFDIR              = 0x4000;
inline constexpr uint16_t EXT_S_IFCHR              = 0x2000;
inline constexpr uint16_t EXT_S_IFIFO              = 0x1000;

// Ext Inode Flags
inline constexpr uint32_t EXT4_EXTENTS_FL          = 0x00080000; // Inode uses extent trees

// Feature Compatibility Flags
inline constexpr uint32_t EXT2_FEATURE_COMPAT_DIR_PREALLOC   = 0x0001;
inline constexpr uint32_t EXT3_FEATURE_COMPAT_HAS_JOURNAL    = 0x0004;
inline constexpr uint32_t EXT2_FEATURE_COMPAT_EXT_ATTR       = 0x0008;
inline constexpr uint32_t EXT2_FEATURE_COMPAT_DIR_INDEX      = 0x0020;

// Feature Read-Only Compatibility Flags
inline constexpr uint32_t EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x0001;
inline constexpr uint32_t EXT2_FEATURE_RO_COMPAT_LARGE_FILE   = 0x0002;
inline constexpr uint32_t EXT2_FEATURE_RO_COMPAT_BTREE_DIR    = 0x0004;
inline constexpr uint32_t EXT4_FEATURE_RO_COMPAT_HUGE_FILE    = 0x0008;
inline constexpr uint32_t EXT4_FEATURE_RO_COMPAT_GDT_CSUM     = 0x0010;
inline constexpr uint32_t EXT4_FEATURE_RO_COMPAT_DIR_NLINK    = 0x0020;
inline constexpr uint32_t EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE  = 0x0040;

// Feature Incompatibility Flags
inline constexpr uint32_t EXT2_FEATURE_INCOMPAT_FILETYPE      = 0x0002;
inline constexpr uint32_t EXT3_FEATURE_INCOMPAT_RECOVER       = 0x0004;
inline constexpr uint32_t EXT3_FEATURE_INCOMPAT_JOURNAL_DEV   = 0x0008;
inline constexpr uint32_t EXT2_FEATURE_INCOMPAT_META_BG       = 0x0010;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_EXTENTS       = 0x0040;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_64BIT         = 0x0080;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_MMP           = 0x0100;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_FLEX_BG       = 0x0200;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_EA_INODE      = 0x0400;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_DIRDATA       = 0x1000;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_CSUM_SEED     = 0x2000;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_LARGEDIR      = 0x4000;
inline constexpr uint32_t EXT4_FEATURE_INCOMPAT_INLINE_DATA   = 0x8000;

// Ext4 Directory Entry File Types
inline constexpr uint8_t EXT4_FT_UNKNOWN  = 0;
inline constexpr uint8_t EXT4_FT_REG_FILE = 1;
inline constexpr uint8_t EXT4_FT_DIR      = 2;
inline constexpr uint8_t EXT4_FT_CHRDEV   = 3;
inline constexpr uint8_t EXT4_FT_BLKDEV   = 4;
inline constexpr uint8_t EXT4_FT_FIFO     = 5;
inline constexpr uint8_t EXT4_FT_SOCK     = 6;
inline constexpr uint8_t EXT4_FT_SYMLINK  = 7;

#pragma pack(push, 1)

// ============================================================================
// 2. On-Disk Structures
// ============================================================================

/**
 * @brief Ext2 / Ext3 / Ext4 Superblock (1024 bytes)
 */
struct ExtSuperblock {
    uint32_t s_inodes_count;          // Total inode count
    uint32_t s_blocks_count_lo;       // Total block count (low 32 bits)
    uint32_t s_r_blocks_count_lo;     // Reserved block count
    uint32_t s_free_blocks_count_lo;  // Free block count (low 32 bits)
    uint32_t s_free_inodes_count;     // Free inode count
    uint32_t s_first_data_block;      // First data block (0 or 1)
    uint32_t s_log_block_size;        // Block size = 1024 << s_log_block_size
    uint32_t s_log_cluster_size;      // Cluster size
    uint32_t s_blocks_per_group;      // Blocks per group
    uint32_t s_clusters_per_group;    // Clusters per group
    uint32_t s_inodes_per_group;      // Inodes per group
    uint32_t s_mtime;                 // Mount time
    uint32_t s_wtime;                 // Write time
    uint16_t s_mnt_count;             // Mount count
    int16_t  s_max_mnt_count;         // Maximal mount count
    uint16_t s_magic;                 // Magic signature (0xEF53)
    uint16_t s_state;                 // File system state
    uint16_t s_errors;                // Behaviour when detecting errors
    uint16_t s_minor_rev_level;       // Minor revision level
    uint32_t s_lastcheck;             // Time of last check
    uint32_t s_checkinterval;         // Max. time between checks
    uint32_t s_creator_os;            // OS creator
    uint32_t s_rev_level;             // Revision level (0 = original, 1 = dynamic)
    uint16_t s_def_resuid;            // Default uid for reserved blocks
    uint16_t s_def_resgid;            // Default gid for reserved blocks

    // Dynamic Revision Fields (s_rev_level >= 1)
    uint32_t s_first_ino;             // First non-reserved inode (usually 11)
    uint16_t s_inode_size;            // Size of on-disk inode structure (128 or 256)
    uint16_t s_block_group_nr;        // Block group # of this superblock
    uint32_t s_feature_compat;        // Compatible feature set
    uint32_t s_feature_incompat;      // Incompatible feature set
    uint32_t s_feature_ro_compat;     // Readonly-compatible feature set
    uint8_t  s_uuid[16];              // 128-bit filesystem UUID
    char     s_volume_name[16];       // Volume label
    char     s_last_mounted[64];      // Directory where last mounted
    uint32_t s_algorithm_usage_bitmap;// For compression
    uint8_t  s_prealloc_blocks;       // Preallocated blocks
    uint8_t  s_prealloc_dir_blocks;   // Preallocated dir blocks
    uint16_t s_reserved_gdt_blocks;   // Number of reserved GDT blocks
    uint8_t  s_journal_uuid[16];      // Journal UUID
    uint32_t s_journal_inum;          // Journal inode
    uint32_t s_journal_dev;           // Journal device
    uint32_t s_last_orphan;           // Head of orphan inode list
    uint32_t s_hash_seed[4];          // HTree hash seed
    uint8_t  s_def_hash_version;      // Default hash version
    uint8_t  s_jnl_backup_type;       // Journal backup type
    uint16_t s_desc_size;             // Size of group descriptor (if 64bit, 64)
    uint32_t s_default_mount_opts;    // Default mount options
    uint32_t s_first_meta_bg;         // First metablock group
    uint32_t s_mkfs_time;             // Filesystem creation timestamp
    uint32_t s_jnl_blocks[17];        // Journal inode backup
    uint32_t s_blocks_count_hi;       // High 32 bits of block count
    uint32_t s_r_blocks_count_hi;     // High 32 bits of reserved block count
    uint32_t s_free_blocks_count_hi;  // High 32 bits of free block count
    uint16_t s_min_extra_isize;       // Minimum extra inode size
    uint16_t s_want_extra_isize;      // Desired extra inode size
    uint32_t s_flags;                 // Miscellaneous flags
    uint16_t s_raid_stride;           // RAID stride
    uint16_t s_mmp_interval;          // MMP check interval
    uint64_t s_mmp_block;             // MMP block
    uint32_t s_raid_stripe_width;     // RAID stripe width
    uint8_t  s_log_groups_per_flex;   // Flex group size
    uint8_t  s_checksum_type;         // Metadata checksum type
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;        // Kilobytes written
    uint8_t  s_padding[640];          // Remaining bytes up to 1024
};
static_assert(sizeof(ExtSuperblock) == 1024, "ExtSuperblock size must be exactly 1024 bytes");

/**
 * @brief Ext2/3/4 Block Group Descriptor (32 or 64 bytes)
 */
struct ExtBlockGroupDescriptor {
    uint32_t bg_block_bitmap_lo;      // Block bitmap block
    uint32_t bg_inode_bitmap_lo;      // Inode bitmap block
    uint32_t bg_inode_table_lo;       // Inode table starting block
    uint16_t bg_free_blocks_count_lo; // Free blocks count
    uint16_t bg_free_inodes_count_lo; // Free inodes count
    uint16_t bg_used_dirs_count_lo;   // Directories count
    uint16_t bg_flags;                // Block group flags
    uint32_t bg_exclude_bitmap_lo;    // Snapshot exclusion bitmap
    uint16_t bg_block_bitmap_csum_lo; // Block bitmap checksum
    uint16_t bg_inode_bitmap_csum_lo; // Inode bitmap checksum
    uint16_t bg_itable_unused_lo;     // Unused inodes count
    uint16_t bg_checksum;             // crc16(sb_uuid + group_num + desc)

    // 64-bit feature extensions
    uint32_t bg_block_bitmap_hi;      // High 32 bits of block bitmap block
    uint32_t bg_inode_bitmap_hi;      // High 32 bits of inode bitmap block
    uint32_t bg_inode_table_hi;       // High 32 bits of inode table block
    uint16_t bg_free_blocks_count_hi; // High 16 bits of free blocks
    uint16_t bg_free_inodes_count_hi; // High 16 bits of free inodes
    uint16_t bg_used_dirs_count_hi;   // High 16 bits of directories
    uint16_t bg_itable_unused_hi;     // High 16 bits of unused inodes
    uint32_t bg_exclude_bitmap_hi;    // High 32 bits of snapshot exclusion bitmap
    uint16_t bg_block_bitmap_csum_hi; // High 16 bits of block bitmap checksum
    uint16_t bg_inode_bitmap_csum_hi; // High 16 bits of inode bitmap checksum
    uint32_t bg_reserved;
};
static_assert(sizeof(ExtBlockGroupDescriptor) == 64, "ExtBlockGroupDescriptor size must be 64 bytes");

/**
 * @brief Ext2/3/4 On-Disk Inode Structure (128 bytes base + extra fields = 256 bytes)
 */
struct ExtInode {
    uint16_t i_mode;                  // File type and access rights
    uint16_t i_uid;                   // Low 16 bits of Owner Uid
    uint32_t i_size_lo;               // Size in bytes (low 32 bits)
    uint32_t i_atime;                 // Access time
    uint32_t i_ctime;                 // Inode change time
    uint32_t i_mtime;                 // Modification time
    uint32_t i_dtime;                 // Deletion time
    uint16_t i_gid;                   // Low 16 bits of Group Id
    uint16_t i_links_count;           // Links count
    uint32_t i_blocks_lo;             // Blocks count (in 512-byte units)
    uint32_t i_flags;                 // File flags (EXT4_EXTENTS_FL = 0x80000)
    uint32_t i_osd1;                  // OS dependent 1
    uint32_t i_block[15];             // Pointers to blocks or Extent Header (60 bytes)
    uint32_t i_generation;            // File version (for NFS)
    uint32_t i_file_acl_lo;           // File ACL
    uint32_t i_size_high;             // High 32 bits of size (for regular files)
    uint32_t i_obso_faddr;            // Obsoleted fragment address
    uint32_t i_osd2[3];               // OS dependent 2

    // Extra fields in modern 256-byte ext4 inodes
    uint16_t i_extra_isize;           // Size of extra fields
    uint16_t i_checksum_hi;           // High 16 bits of checksum
    uint32_t i_ctime_extra;           // Extra change time (nanoseconds)
    uint32_t i_mtime_extra;           // Extra modification time (nanoseconds)
    uint32_t i_atime_extra;           // Extra access time (nanoseconds)
    uint32_t i_crtime;                // File creation time
    uint32_t i_crtime_extra;          // Extra creation time
    uint32_t i_version_hi;            // High 32 bits of 64-bit version
    uint32_t i_projid;                // Project ID
    uint8_t  i_pad[96];               // Padding to 256 bytes
};
static_assert(sizeof(ExtInode) == 256, "ExtInode size must be 256 bytes");

/**
 * @brief Ext4 Extent Tree Header (12 bytes)
 */
struct Ext4ExtentHeader {
    uint16_t eh_magic;                // Magic number (0xF30A)
    uint16_t eh_entries;              // Valid entries count
    uint16_t eh_max;                  // Capacity of entries
    uint16_t eh_depth;                // Tree depth: 0 = leaf, >0 = internal index node
    uint32_t eh_generation;           // Generation of extent tree
};
static_assert(sizeof(Ext4ExtentHeader) == 12, "Ext4ExtentHeader size must be 12 bytes");

/**
 * @brief Ext4 Extent Internal Index Node (depth > 0)
 */
struct Ext4ExtentIdx {
    uint32_t ei_block;                // Covers file blocks starting here
    uint32_t ei_leaf_lo;              // Low 32 bits of physical block of next level
    uint16_t ei_leaf_hi;              // High 16 bits of physical block
    uint16_t ei_unused;
};
static_assert(sizeof(Ext4ExtentIdx) == 12, "Ext4ExtentIdx size must be 12 bytes");

/**
 * @brief Ext4 Extent Leaf Node (depth == 0)
 */
struct Ext4Extent {
    uint32_t ee_block;                // First file logical block number
    uint16_t ee_len;                  // Number of covered blocks (<= 32768 initialized)
    uint16_t ee_start_hi;             // High 16 bits of physical block
    uint32_t ee_start_lo;             // Low 32 bits of physical block
};
static_assert(sizeof(Ext4Extent) == 12, "Ext4Extent size must be 12 bytes");

/**
 * @brief Ext4 Directory Entry Header (Variable Length)
 */
struct Ext4DirEntry2 {
    uint32_t inode;                   // Inode number (0 = free / deleted)
    uint16_t rec_len;                 // Record length to next entry
    uint8_t  name_len;                // Name length
    uint8_t  file_type;               // File type (EXT4_FT_*)
    char     name[1];                 // Filename bytes (not null-terminated)
};

#pragma pack(pop)

// ============================================================================
// 3. Storage Abstraction & Logical Block Device Interface
// ============================================================================

class IExtBlockDevice {
public:
    virtual ~IExtBlockDevice() = default;
    [[nodiscard]] virtual uint64_t getSectorCount() const noexcept = 0;
    [[nodiscard]] virtual uint32_t getSectorSize() const noexcept = 0;
    virtual bool readBytes(uint64_t byteOffset, size_t length, void* buffer) = 0;
};

/**
 * @brief In-Memory Block Device for testing and synthetic ext4 image mounting
 */
class MemoryExtBlockDevice : public IExtBlockDevice {
public:
    MemoryExtBlockDevice(std::vector<uint8_t> data, uint32_t sectorSize = 512)
        : data_(std::move(data)), sectorSize_(sectorSize) {}

    [[nodiscard]] uint64_t getSectorCount() const noexcept override {
        return data_.size() / sectorSize_;
    }

    [[nodiscard]] uint32_t getSectorSize() const noexcept override {
        return sectorSize_;
    }

    bool readBytes(uint64_t byteOffset, size_t length, void* buffer) override {
        if (!buffer || byteOffset + length > data_.size()) {
            return false;
        }
        std::memcpy(buffer, data_.data() + byteOffset, length);
        return true;
    }

    std::vector<uint8_t>& getRawData() noexcept { return data_; }

private:
    std::vector<uint8_t> data_;
    uint32_t sectorSize_{512};
};

// ============================================================================
// 4. Directory & File Metadata Representation
// ============================================================================

struct ExtFsDirEntry {
    uint32_t inode{0};
    uint8_t  fileType{EXT4_FT_UNKNOWN};
    std::string name;
    std::wstring wideName;
    uint64_t fileSize{0};
    uint16_t mode{0};
    uint64_t creationTime{0}; // NT 100ns FILETIME
    uint64_t lastWriteTime{0};// NT 100ns FILETIME
    bool isDirectory{false};
    bool isSymlink{false};
};

// ============================================================================
// 5. Ext2/3/4 Read-Only Engine (ExtFileSystem)
// ============================================================================

class ExtFileSystem {
public:
    ExtFileSystem() = default;
    ~ExtFileSystem() = default;

    /**
     * @brief Mount an ext2/3/4 block device in strict read-only mode
     */
    NTSTATUS mount(std::shared_ptr<IExtBlockDevice> device);

    [[nodiscard]] bool isMounted() const noexcept { return mounted_; }
    [[nodiscard]] uint32_t getBlockSize() const noexcept { return blockSize_; }
    [[nodiscard]] uint32_t getInodeSize() const noexcept { return inodeSize_; }
    [[nodiscard]] uint32_t getInodesPerGroup() const noexcept { return inodesPerGroup_; }
    [[nodiscard]] uint32_t getBlocksPerGroup() const noexcept { return blocksPerGroup_; }
    [[nodiscard]] uint32_t getGroupCount() const noexcept { return groupCount_; }
    [[nodiscard]] const std::string& getVolumeLabel() const noexcept { return volumeLabel_; }
    [[nodiscard]] const ExtSuperblock& getSuperblock() const noexcept { return sb_; }

    /**
     * @brief Read on-disk inode by 1-based inode number
     */
    NTSTATUS readInode(uint32_t inodeNum, ExtInode& outInode);

    /**
     * @brief Read bytes from an inode (resolves extents or direct/indirect blocks)
     */
    NTSTATUS readFileBytes(const ExtInode& inode, uint64_t offset, size_t length, void* buffer, size_t* bytesRead);

    /**
     * @brief List directory contents by inode
     */
    NTSTATUS listDirectory(uint32_t dirInodeNum, std::vector<ExtFsDirEntry>& entries);

    /**
     * @brief Resolve path (e.g. "/etc/os-release" or "etc/os-release") to inode number
     */
    NTSTATUS resolvePath(std::string_view path, uint32_t& outInodeNum, ExtInode* outInode = nullptr);

    /**
     * @brief Resolve Win32 wide path (case-insensitive lookup)
     */
    NTSTATUS resolveWin32Path(std::wstring_view path, uint32_t& outInodeNum, ExtInode* outInode = nullptr);

    /**
     * @brief Read fast inline symlink or extent symlink
     */
    NTSTATUS readSymlink(const ExtInode& inode, std::string& target);

    /**
     * @brief Translate Linux Unix timestamp to NT 100ns FILETIME
     */
    static uint64_t unixTimeToNtFileTime(uint32_t sec, uint32_t nsec = 0) noexcept;

private:
    bool readBlock(uint64_t blockNum, void* buffer);
    uint64_t resolveLogicalBlock(const ExtInode& inode, uint32_t logicalBlock);
    uint64_t traverseExtentTree(const uint8_t* extentData, uint32_t logicalBlock);

    std::shared_ptr<IExtBlockDevice> device_{nullptr};
    ExtSuperblock sb_{};
    std::vector<ExtBlockGroupDescriptor> groups_{};
    uint32_t blockSize_{4096};
    uint32_t inodeSize_{256};
    uint32_t inodesPerGroup_{0};
    uint32_t blocksPerGroup_{0};
    uint32_t groupCount_{0};
    uint16_t descSize_{32};
    bool is64Bit_{false};
    bool hasExtents_{false};
    bool mounted_{false};
    std::string volumeLabel_;
    std::mutex fsMutex_;
};

// ============================================================================
// 6. NT Installable File System (IFS) Driver Wrapper
// ============================================================================

class ExtFsDriver {
public:
    static ExtFsDriver& get() noexcept {
        static ExtFsDriver s_instance;
        return s_instance;
    }

    void initialize();
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    /**
     * @brief Register mounted Linux volume under NT namespace (e.g. \Device\ExtFs0)
     */
    NTSTATUS registerVolume(std::shared_ptr<ExtFileSystem> extFs, std::wstring_view volumeName = L"\\Device\\ExtFs0");

    /**
     * @brief Mount volume onto a Win32 drive letter (e.g. L"E:")
     */
    NTSTATUS mountDrive(std::wstring_view driveLetter, std::shared_ptr<ExtFileSystem> extFs);

    [[nodiscard]] std::shared_ptr<ExtFileSystem> getFileSystem(std::wstring_view driveLetter) const;

    // Dispatch routines for NT IRPs
    static NtStatus dispatchCreate(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchRead(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchWrite(io::DeviceObject* device, io::Irp* irp); // Always returns NtStatus::MediaWriteProtected
    static NtStatus dispatchClose(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchDirectoryControl(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchQueryInformation(io::DeviceObject* device, io::Irp* irp);
    static NtStatus dispatchFileSystemControl(io::DeviceObject* device, io::Irp* irp);

private:
    ExtFsDriver() = default;
    bool initialized_{false};
    std::unique_ptr<io::DriverObject> driverObject_{nullptr};
    std::unordered_map<std::wstring, std::shared_ptr<ExtFileSystem>> mountedDrives_;
    mutable std::mutex driverMutex_;
};

} // namespace micant::fs::ext

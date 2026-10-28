#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <span>
#include <cstring>
#include <algorithm>
#include <cwctype>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/executive/io.hpp"
#include "micant/km/executive/ob.hpp"
#include "micant/km/drivers/storage/storage.hpp"
#include "micant/km/drivers/fs/fat32.hpp"
#include "micant/km/drivers/fs/npfs.hpp"
#include "micant/km/drivers/fs/ntfs.hpp"
#include "micant/km/drivers/fs/exfat.hpp"
#include "micant/km/drivers/fs/cdfs.hpp"
#include "micant/km/drivers/fs/refs.hpp"
#include "micant/km/drivers/fs/zfs.hpp"
#include "micant/km/drivers/fs/btrfs.hpp"
#include "micant/km/drivers/fs/extfs.hpp"
#include "micant/km/drivers/fs/xfs.hpp"
#include "micant/km/drivers/fs/f2fs.hpp"

namespace micant::fs {

// Standard NT File Access Rights
inline constexpr uint32_t FILE_READ_DATA        = 0x0001;
inline constexpr uint32_t FILE_WRITE_DATA       = 0x0002;
inline constexpr uint32_t FILE_APPEND_DATA      = 0x0004;
inline constexpr uint32_t FILE_EXECUTE         = 0x0020;
inline constexpr uint32_t FILE_GENERIC_READ     = 0x00120089;
inline constexpr uint32_t FILE_GENERIC_WRITE    = 0x00120116;
inline constexpr uint32_t FILE_GENERIC_EXECUTE  = 0x001200A0;
inline constexpr uint32_t FILE_ALL_ACCESS       = 0x001F01FF;

// File Creation / Open Dispositions
inline constexpr uint32_t FILE_SUPERSEDE        = 0x00000000;
inline constexpr uint32_t FILE_OPEN             = 0x00000001;
inline constexpr uint32_t FILE_CREATE           = 0x00000002;
inline constexpr uint32_t FILE_OPEN_IF          = 0x00000003;
inline constexpr uint32_t FILE_OVERWRITE        = 0x00000004;
inline constexpr uint32_t FILE_OVERWRITE_IF     = 0x00000005;

// File Attributes
inline constexpr uint32_t FILE_ATTRIBUTE_READONLY  = 0x00000001;
inline constexpr uint32_t FILE_ATTRIBUTE_HIDDEN    = 0x00000002;
inline constexpr uint32_t FILE_ATTRIBUTE_SYSTEM    = 0x00000004;
inline constexpr uint32_t FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
inline constexpr uint32_t FILE_ATTRIBUTE_NORMAL    = 0x00000080;

// File Object Flags
inline constexpr uint32_t FO_SYNCHRONOUS_IO    = 0x00000010;
inline constexpr uint32_t FO_ALERTABLE_IO      = 0x00000020;
inline constexpr uint32_t FO_NO_INTERMEDIATE_BUFFERING = 0x00000040;

/**
 * @brief NT File Object (FILE_OBJECT)
 * Clean-room modern C++23 representation of an open file instance.
 */
class FileObject {
public:
    FileObject(
        io::DeviceObject* device,
        std::wstring_view fileName,
        uint32_t accessMask,
        uint32_t flags = FO_SYNCHRONOUS_IO
    ) : deviceObject_(device),
        fileName_(fileName),
        accessMask_(accessMask),
        flags_(flags) {}

    [[nodiscard]] io::DeviceObject* getDeviceObject() const noexcept { return deviceObject_; }
    [[nodiscard]] const std::wstring& getFileName() const noexcept { return fileName_; }
    [[nodiscard]] uint32_t getAccessMask() const noexcept { return accessMask_; }
    [[nodiscard]] uint32_t getFlags() const noexcept { return flags_; }
    [[nodiscard]] int64_t getCurrentByteOffset() const noexcept { return currentByteOffset_; }

    void setCurrentByteOffset(int64_t offset) noexcept { currentByteOffset_ = offset; }
    void advanceByteOffset(int64_t bytes) noexcept { currentByteOffset_ += bytes; }

    [[nodiscard]] bool isDirectory() const noexcept { return isDirectory_; }
    void setDirectory(bool isDir) noexcept { isDirectory_ = isDir; }

    [[nodiscard]] size_t getFileSize() const noexcept { return data_.size(); }
    [[nodiscard]] const std::vector<uint8_t>& getData() const noexcept { return data_; }
    std::vector<uint8_t>& getData() noexcept { return data_; }

    void writeData(size_t offset, std::span<const uint8_t> bytes) {
        if (offset + bytes.size() > data_.size()) {
            data_.resize(offset + bytes.size());
        }
        std::memcpy(data_.data() + offset, bytes.data(), bytes.size());
    }

    [[nodiscard]] void* getFsContext() const noexcept { return fsContext_; }
    void setFsContext(void* ctx) noexcept { fsContext_ = ctx; }

    [[nodiscard]] void* getFsContext2() const noexcept { return fsContext2_; }
    void setFsContext2(void* ctx) noexcept { fsContext2_ = ctx; }

private:
    io::DeviceObject* deviceObject_{nullptr};
    std::wstring fileName_;
    uint32_t accessMask_{0};
    uint32_t flags_{FO_SYNCHRONOUS_IO};
    int64_t currentByteOffset_{0};
    bool isDirectory_{false};
    std::vector<uint8_t> data_;
    void* fsContext_{nullptr};
    void* fsContext2_{nullptr};
};

/**
 * @brief Clean-Room FAT32 / VFS In-Memory Directory Entry
 */
struct VfsEntry {
    std::wstring name;
    uint32_t attributes{FILE_ATTRIBUTE_NORMAL};
    std::vector<uint8_t> content;
    std::unordered_map<std::wstring, std::shared_ptr<VfsEntry>> children;
    bool isDirectory{false};
};

/**
 * @brief Fastfat / Virtual File System Driver
 * Clean-room NT driver supporting mounting, partition navigation, and file operations.
 */
// ============================================================================
// File System Attribute Flags (for NtQueryVolumeInformationFile)
// ============================================================================

inline constexpr uint32_t FS_CASE_SENSITIVE_SEARCH        = 0x00000001;
inline constexpr uint32_t FS_CASE_IS_PRESERVED            = 0x00000002;
inline constexpr uint32_t FS_UNICODE_STORED_ON_DISK       = 0x00000004;
inline constexpr uint32_t FS_PERSISTENT_ACLS              = 0x00000008;
inline constexpr uint32_t FS_FILE_COMPRESSION             = 0x00000010;
inline constexpr uint32_t FS_VOLUME_QUOTAS                = 0x00000020;
inline constexpr uint32_t FS_SUPPORTS_SPARSE_FILES        = 0x00000040;
inline constexpr uint32_t FS_SUPPORTS_REPARSE_POINTS      = 0x00000080;
inline constexpr uint32_t FS_SUPPORTS_OBJECT_IDS          = 0x00010000;
inline constexpr uint32_t FS_SUPPORTS_ENCRYPTION          = 0x00020000;
inline constexpr uint32_t FS_READ_ONLY_VOLUME             = 0x00080000;
inline constexpr uint32_t FS_SUPPORTS_TRANSACTIONS        = 0x00200000;
inline constexpr uint32_t FS_SUPPORTS_USN_JOURNAL         = 0x02000000;
inline constexpr uint32_t FS_SUPPORTS_INTEGRITY_STREAMS   = 0x04000000;
inline constexpr uint32_t FS_SUPPORTS_BLOCK_REFCOUNTING   = 0x08000000;
inline constexpr uint32_t FS_SUPPORTS_COW_SNAPSHOTS       = 0x10000000;
inline constexpr uint32_t FS_SUPPORTS_RAID_PARITY         = 0x20000000;

struct FileSystemVolumeInfo {
    wchar_t driveLetter{L'C'};
    std::wstring volumeLabel{L"MICA_SYS"};
    std::wstring fileSystemName{L"FAT32"};
    std::wstring volumeGuid{L"\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\"};
    std::wstring devicePath{L"\\Device\\Harddisk0\\Partition1"};
    uint32_t serialNumber{0x1337B007};
    uint32_t bytesPerSector{512};
    uint32_t sectorsPerCluster{8};
    uint64_t totalClusters{32768000};
    uint64_t freeClusters{32760000};
    uint32_t maxComponentLength{255};
    uint32_t fileSystemAttributes{FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK};
    bool isReadOnly{false};
    bool isDirty{false};
};

class VirtualFileSystem {
public:
    struct PartitionResolution {
        enum class VolumeType { Fat32, Ntfs, Exfat, Cdfs, Refs, Zfs, Btrfs, Ext4, Xfs, F2fs };
        VolumeType type{VolumeType::Fat32};
        std::wstring relPath;
    };

    struct DriveMapping {
        wchar_t letter{L'C'};
        PartitionResolution::VolumeType type{PartitionResolution::VolumeType::Fat32};
        std::wstring targetDevice{L"\\Device\\Harddisk0\\Partition1"};
        std::wstring volumeGuid;
        std::wstring volumeLabel;
    };

    static VirtualFileSystem& get() {
        static VirtualFileSystem instance;
        instance.initialize();
        return instance;
    }

    NtStatus assignDriveLetter(
        wchar_t letter,
        std::wstring_view targetDevice,
        std::wstring_view volumeGuid = L"",
        std::wstring_view volumeLabel = L""
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(letter));
        if (d < L'A' || d > L'Z') return NtStatus::InvalidParameter;

        std::wstring target(targetDevice);
        PartitionResolution::VolumeType vType = PartitionResolution::VolumeType::Fat32;
        std::wstring vLabel(volumeLabel);
        std::wstring guid(volumeGuid);

        if (target.contains(L"Btrfs") || target.contains(L"btrfs") || target.contains(L"0007")) {
            vType = PartitionResolution::VolumeType::Btrfs;
            if (vLabel.empty()) vLabel = L"MICA_BTRFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0007-0000-0000-000000000007}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Btrfs0";
        } else if (target.contains(L"Ext") || target.contains(L"ext") || target.contains(L"0008")) {
            vType = PartitionResolution::VolumeType::Ext4;
            if (vLabel.empty()) vLabel = L"MICA_EXT4";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0008-0000-0000-000000000008}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\ExtFs0";
        } else if (target.contains(L"Xfs") || target.contains(L"xfs") || target.contains(L"0009")) {
            vType = PartitionResolution::VolumeType::Xfs;
            if (vLabel.empty()) vLabel = L"MICA_XFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0009-0000-0000-000000000009}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Xfs0";
        } else if (target.contains(L"F2fs") || target.contains(L"f2fs") || target.contains(L"0010")) {
            vType = PartitionResolution::VolumeType::F2fs;
            if (vLabel.empty()) vLabel = L"MICA_F2FS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0010-0000-0000-000000000010}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\F2fs0";
        } else if (target.contains(L"Zfs") || target.contains(L"tank") || target.contains(L"0006")) {
            vType = PartitionResolution::VolumeType::Zfs;
            if (vLabel.empty()) vLabel = L"MICA_ZFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0006-0000-0000-000000000006}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\ZfsPool_Storage";
        } else if (target.contains(L"Partition2") || target.contains(L"Ntfs") || target.contains(L"NTFS") || target.contains(L"0002")) {
            vType = PartitionResolution::VolumeType::Ntfs;
            if (vLabel.empty()) vLabel = L"MICA_NTFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Harddisk0\\Partition2";
        } else if (target.contains(L"Partition3") || target.contains(L"ExFat") || target.contains(L"exfat") || target.contains(L"0003")) {
            vType = PartitionResolution::VolumeType::Exfat;
            if (vLabel.empty()) vLabel = L"MICA_EXFAT";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0003-0000-0000-000000000003}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Harddisk0\\Partition3";
        } else if (target.contains(L"CdRom") || target.contains(L"Cdfs") || target.contains(L"cdfs") || target.contains(L"0004")) {
            vType = PartitionResolution::VolumeType::Cdfs;
            if (vLabel.empty()) vLabel = L"MICA_CDFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0004-0000-0000-000000000004}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\CdRom0";
        } else if (target.contains(L"Partition4") || target.contains(L"Refs") || target.contains(L"REFS") || target.contains(L"0005")) {
            vType = PartitionResolution::VolumeType::Refs;
            if (vLabel.empty()) vLabel = L"MICA_REFS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0005-0000-0000-000000000005}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Harddisk0\\Partition4";
        } else if (target.contains(L"Partition1") || target.contains(L"Fat") || target.contains(L"FAT") || target.contains(L"0001")) {
            vType = PartitionResolution::VolumeType::Fat32;
            if (vLabel.empty()) vLabel = L"MICA_SYS";
            if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\";
            if (target.empty() || target.contains(L"Volume{")) target = L"\\Device\\Harddisk0\\Partition1";
        } else {
            // Target is unspecified or arbitrary, fall back to drive letter defaults
            if (d == L'B') {
                vType = PartitionResolution::VolumeType::Btrfs;
                if (vLabel.empty()) vLabel = L"MICA_BTRFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0007-0000-0000-000000000007}\\";
                if (target.empty()) target = L"\\Device\\Btrfs0";
            } else if (d == L'X') {
                vType = PartitionResolution::VolumeType::Ext4;
                if (vLabel.empty()) vLabel = L"MICA_EXT4";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0008-0000-0000-000000000008}\\";
                if (target.empty()) target = L"\\Device\\ExtFs0";
            } else if (d == L'S') {
                vType = PartitionResolution::VolumeType::Xfs;
                if (vLabel.empty()) vLabel = L"MICA_XFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0009-0000-0000-000000000009}\\";
                if (target.empty()) target = L"\\Device\\Xfs0";
            } else if (d == L'Y') {
                vType = PartitionResolution::VolumeType::F2fs;
                if (vLabel.empty()) vLabel = L"MICA_F2FS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0010-0000-0000-000000000010}\\";
                if (target.empty()) target = L"\\Device\\F2fs0";
            } else if (d == L'Z') {
                vType = PartitionResolution::VolumeType::Zfs;
                if (vLabel.empty()) vLabel = L"MICA_ZFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0006-0000-0000-000000000006}\\";
                if (target.empty()) target = L"\\Device\\ZfsPool_Storage";
            } else if (d == L'D') {
                vType = PartitionResolution::VolumeType::Ntfs;
                if (vLabel.empty()) vLabel = L"MICA_NTFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\";
                if (target.empty()) target = L"\\Device\\Harddisk0\\Partition2";
            } else if (d == L'E') {
                vType = PartitionResolution::VolumeType::Exfat;
                if (vLabel.empty()) vLabel = L"MICA_EXFAT";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0003-0000-0000-000000000003}\\";
                if (target.empty()) target = L"\\Device\\Harddisk0\\Partition3";
            } else if (d == L'F') {
                vType = PartitionResolution::VolumeType::Cdfs;
                if (vLabel.empty()) vLabel = L"MICA_CDFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0004-0000-0000-000000000004}\\";
                if (target.empty()) target = L"\\Device\\CdRom0";
            } else if (d == L'G') {
                vType = PartitionResolution::VolumeType::Refs;
                if (vLabel.empty()) vLabel = L"MICA_REFS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0005-0000-0000-000000000005}\\";
                if (target.empty()) target = L"\\Device\\Harddisk0\\Partition4";
            } else {
                vType = PartitionResolution::VolumeType::Fat32;
                if (vLabel.empty()) vLabel = L"MICA_SYS";
                if (guid.empty()) guid = L"\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\";
                if (target.empty()) target = L"\\Device\\Harddisk0\\Partition1";
            }
        }

        driveMappings_[d] = DriveMapping{
            .letter = d,
            .type = vType,
            .targetDevice = target,
            .volumeGuid = guid,
            .volumeLabel = vLabel
        };

        // Update Object Manager symbolic link \DosDevices\<d>: -> target
        std::wstring dosName = std::wstring(L"\\DosDevices\\") + d + L":";
        ob::ObjectNamespace::get().createSymbolicLink(dosName, target);

        return NtStatus::Success;
    }

    NtStatus removeDriveLetter(wchar_t letter) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(letter));
        driveMappings_.erase(d);

        std::wstring dosName = std::wstring(L"\\DosDevices\\") + d + L":";
        ob::ObjectNamespace::get().removeSymbolicLink(dosName);
        return NtStatus::Success;
    }

    [[nodiscard]] bool isDriveLetterAssigned(wchar_t letter) const {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(letter));
        return driveMappings_.contains(d);
    }

    [[nodiscard]] std::vector<wchar_t> getMountedDriveLetters() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<wchar_t> letters;
        for (const auto& [l, _] : driveMappings_) {
            letters.push_back(l);
        }
        std::sort(letters.begin(), letters.end());
        return letters;
    }

    [[nodiscard]] std::wstring getDeviceForDriveLetter(wchar_t letter) const {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(letter));
        auto it = driveMappings_.find(d);
        if (it != driveMappings_.end()) return it->second.targetDevice;
        return L"";
    }

    [[nodiscard]] wchar_t getDriveLetterForDevice(std::wstring_view devicePath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [l, m] : driveMappings_) {
            if (m.targetDevice == devicePath) return l;
        }
        return L'\0';
    }

    [[nodiscard]] wchar_t getDriveLetterForVolumeGuid(std::wstring_view volumeGuid) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [l, m] : driveMappings_) {
            if (m.volumeGuid == volumeGuid) return l;
        }
        return L'\0';
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;

        // Register FastFAT driver object
        fastFatDriver_ = std::make_unique<io::DriverObject>();
        fastFatDriver_->driverName = L"\\Driver\\Fastfat";

        // Setup dispatch routines
        fastFatDriver_->setDispatch(io::IRP_MJ_CREATE, &VirtualFileSystem::dispatchCreate);
        fastFatDriver_->setDispatch(io::IRP_MJ_READ, &VirtualFileSystem::dispatchRead);
        fastFatDriver_->setDispatch(io::IRP_MJ_WRITE, &VirtualFileSystem::dispatchWrite);
        fastFatDriver_->setDispatch(io::IRP_MJ_CLOSE, &VirtualFileSystem::dispatchClose);
        fastFatDriver_->setDispatch(io::IRP_MJ_FILE_SYSTEM_CONTROL, &VirtualFileSystem::dispatchFileSystemControl);

        fastIoTable_.fastIoRead = [](void* fileObject, LargeInteger* offset, uint32_t length, bool, void* buffer, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            if (!fileObject || !buffer || !ioStatus) return false;
            auto* fileObj = static_cast<FileObject*>(fileObject);
            uint32_t bytesRead = 0;
            NtStatus st = VirtualFileSystem::get().readFile(fileObj, buffer, length, offset, bytesRead);
            ioStatus->status = st;
            ioStatus->information = bytesRead;
            return NT_SUCCESS(st);
        };
        fastIoTable_.fastIoWrite = [](void* fileObject, LargeInteger* offset, uint32_t length, bool, const void* buffer, IoStatusBlock* ioStatus, io::DeviceObject*) -> bool {
            if (!fileObject || !buffer || !ioStatus) return false;
            auto* fileObj = static_cast<FileObject*>(fileObject);
            uint32_t bytesWritten = 0;
            NtStatus st = VirtualFileSystem::get().writeFile(fileObj, buffer, length, offset, bytesWritten);
            ioStatus->status = st;
            ioStatus->information = bytesWritten;
            return NT_SUCCESS(st);
        };
        fastFatDriver_->fastIoDispatch = &fastIoTable_;

        // Create partition device node
        partitionDevice_ = io::IoManager::get().createDevice(
            fastFatDriver_.get(),
            L"\\Device\\Harddisk0\\Partition1",
            io::DeviceType::FileSystem
        );

        // Initialize IPC File Systems
        npfs::NamedPipeFileSystem::get().initialize();
        npfs::MailslotFileSystem::get().initialize();

        // Initialize root directory hierarchy
        rootEntry_ = std::make_shared<VfsEntry>();
        rootEntry_->name = L"";
        rootEntry_->isDirectory = true;
        rootEntry_->attributes = FILE_ATTRIBUTE_DIRECTORY;

        // Populate standard system directories
        createDirectoryInternal(L"Windows");
        createDirectoryInternal(L"Windows\\System32");
        createDirectoryInternal(L"Windows\\System32\\drivers");
        createDirectoryInternal(L"Windows\\System32\\config");
        createDirectoryInternal(L"Program Files");
        createDirectoryInternal(L"Users");
        createDirectoryInternal(L"Users\\Default");

        // Seed core NT system image stubs into System32
        std::string ntdllStub = "MZ-MICANT-NTDLL64-CLEANROOM-CORE-EXPORTS";
        std::vector<uint8_t> ntdllBytes(ntdllStub.begin(), ntdllStub.end());
        createFileInternal(L"Windows\\System32\\ntdll.dll", ntdllBytes);

        std::string kernel32Stub = "MZ-MICANT-KERNEL32-CLEANROOM-STUB";
        std::vector<uint8_t> kernel32Bytes(kernel32Stub.begin(), kernel32Stub.end());
        createFileInternal(L"Windows\\System32\\kernel32.dll", kernel32Bytes);

        std::string systemHiveStub = "regf-MICANT-SYSTEM-HIVE-V1";
        std::vector<uint8_t> hiveBytes(systemHiveStub.begin(), systemHiveStub.end());
        createFileInternal(L"Windows\\System32\\config\\SYSTEM", hiveBytes);

        // Seed standard initial drive mappings (C: through G:, Z:)
        if (driveMappings_.empty()) {
            driveMappings_[L'C'] = DriveMapping{ L'C', PartitionResolution::VolumeType::Fat32, L"\\Device\\Harddisk0\\Partition1", L"\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\", L"MICA_SYS" };
            driveMappings_[L'D'] = DriveMapping{ L'D', PartitionResolution::VolumeType::Ntfs, L"\\Device\\Harddisk0\\Partition2", L"\\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\", L"MICA_NTFS" };
            driveMappings_[L'E'] = DriveMapping{ L'E', PartitionResolution::VolumeType::Exfat, L"\\Device\\Harddisk0\\Partition3", L"\\\\?\\Volume{a4bf011e-0003-0000-0000-000000000003}\\", L"MICA_EXFAT" };
            driveMappings_[L'F'] = DriveMapping{ L'F', PartitionResolution::VolumeType::Cdfs, L"\\Device\\CdRom0", L"\\\\?\\Volume{a4bf011e-0004-0000-0000-000000000004}\\", L"MICA_CDFS" };
            driveMappings_[L'G'] = DriveMapping{ L'G', PartitionResolution::VolumeType::Refs, L"\\Device\\Harddisk0\\Partition4", L"\\\\?\\Volume{a4bf011e-0005-0000-0000-000000000005}\\", L"MICA_REFS" };
            driveMappings_[L'Z'] = DriveMapping{ L'Z', PartitionResolution::VolumeType::Zfs, L"\\Device\\ZfsPool_Storage", L"\\\\?\\Volume{a4bf011e-0006-0000-0000-000000000006}\\", L"MICA_ZFS" };
        }

        auto& obNs = ob::ObjectNamespace::get();
        for (const auto& [l, m] : driveMappings_) {
            std::wstring dosName = std::wstring(L"\\DosDevices\\") + l + L":";
            obNs.createSymbolicLink(dosName, m.targetDevice);
        }

        initialized_ = true;
    }

    [[nodiscard]] io::DeviceObject* getPartitionDevice() const noexcept {
        return partitionDevice_.get();
    }

    NtStatus mountBlockDevice(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        mountedDevice_ = device;
        fat32Fs_ = std::make_shared<fat32::Fat32FileSystem>();
        NtStatus st = fat32Fs_->mount(mountedDevice_);
        if (!NT_SUCCESS(st)) {
            fat32Fs_.reset();
            mountedDevice_.reset();
            return st;
        }

        // Initialize canonical NT Volume Parameter Block (VPB)
        vpb_ = std::make_unique<io::Vpb>();
        vpb_->flags = io::VPB_MOUNTED;
        vpb_->realDevice = partitionDevice_.get();
        vpb_->deviceObject = partitionDevice_.get();
        vpb_->serialNumber = 0x53494D53;
        std::wcsncpy(vpb_->volumeLabel, L"MICANT_SYS", 31);
        vpb_->volumeLabelLength = 10 * sizeof(wchar_t);
        if (partitionDevice_) {
            partitionDevice_->vpb = vpb_.get();
        }

        return NtStatus::Success;
    }

    [[nodiscard]] io::Vpb* getVpb() const noexcept {
        return vpb_.get();
    }

    [[nodiscard]] std::shared_ptr<fat32::Fat32FileSystem> getMountedFat32() const noexcept {
        return fat32Fs_;
    }

    [[nodiscard]] std::shared_ptr<storage::IBlockDevice> getMountedBlockDevice() const noexcept {
        return mountedDevice_;
    }

    NtStatus mountNtfs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        ntfsFs_ = std::make_shared<ntfs::NtfsFileSystem>();
        NtStatus st = ntfsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            ntfsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedNtfs(std::shared_ptr<ntfs::NtfsFileSystem> ntfs) {
        std::lock_guard<std::mutex> lock(mutex_);
        ntfsFs_ = ntfs;
    }

    [[nodiscard]] std::shared_ptr<ntfs::NtfsFileSystem> getMountedNtfs() const noexcept {
        return ntfsFs_;
    }

    NtStatus mountExfat(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        exfatFs_ = std::make_shared<exfat::ExfatFileSystem>();
        NtStatus st = exfatFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            exfatFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedExfat(std::shared_ptr<exfat::ExfatFileSystem> exfat) {
        std::lock_guard<std::mutex> lock(mutex_);
        exfatFs_ = exfat;
    }

    [[nodiscard]] std::shared_ptr<exfat::ExfatFileSystem> getMountedExfat() const noexcept {
        return exfatFs_;
    }

    NtStatus mountCdfs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        cdfsFs_ = std::make_shared<cdfs::CdfsFileSystem>();
        NtStatus st = cdfsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            cdfsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedCdfs(std::shared_ptr<cdfs::CdfsFileSystem> cdfs) {
        std::lock_guard<std::mutex> lock(mutex_);
        cdfsFs_ = cdfs;
    }

    [[nodiscard]] std::shared_ptr<cdfs::CdfsFileSystem> getMountedCdfs() const noexcept {
        return cdfsFs_;
    }

    NtStatus mountRefs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        refsFs_ = std::make_shared<refs::RefsFileSystem>();
        NtStatus st = refsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            refsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedRefs(std::shared_ptr<refs::RefsFileSystem> refs) {
        std::lock_guard<std::mutex> lock(mutex_);
        refsFs_ = refs;
    }

    [[nodiscard]] std::shared_ptr<refs::RefsFileSystem> getMountedRefs() const noexcept {
        return refsFs_;
    }

    void setMountedZfs(std::shared_ptr<zfs::ZfsFileSystem> zfs) {
        std::lock_guard<std::mutex> lock(mutex_);
        zfsFs_ = zfs;
    }

    [[nodiscard]] std::shared_ptr<zfs::ZfsFileSystem> getMountedZfs() const noexcept {
        return zfsFs_;
    }

    NtStatus mountBtrfs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        btrfsFs_ = std::make_shared<btrfs::BtrfsFileSystem>();
        NtStatus st = btrfsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            btrfsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedBtrfs(std::shared_ptr<btrfs::BtrfsFileSystem> btrfs) {
        std::lock_guard<std::mutex> lock(mutex_);
        btrfsFs_ = btrfs;
    }

    [[nodiscard]] std::shared_ptr<btrfs::BtrfsFileSystem> getMountedBtrfs() const noexcept {
        return btrfsFs_;
    }

    NtStatus mountExt4(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        extFs_ = std::make_shared<fs::ext::ExtFileSystem>();
        NtStatus st = extFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            extFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedExt(std::shared_ptr<fs::ext::ExtFileSystem> ext) {
        std::lock_guard<std::mutex> lock(mutex_);
        extFs_ = ext;
    }

    [[nodiscard]] std::shared_ptr<fs::ext::ExtFileSystem> getMountedExt() const noexcept {
        return extFs_;
    }

    NtStatus mountXfs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        xfsFs_ = std::make_shared<xfs::XfsFileSystem>();
        NtStatus st = xfsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            xfsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedXfs(std::shared_ptr<xfs::XfsFileSystem> xfs) {
        std::lock_guard<std::mutex> lock(mutex_);
        xfsFs_ = xfs;
    }

    [[nodiscard]] std::shared_ptr<xfs::XfsFileSystem> getMountedXfs() const noexcept {
        return xfsFs_;
    }

    NtStatus mountF2fs(std::shared_ptr<storage::IBlockDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device) return NtStatus::InvalidParameter;
        f2fsFs_ = std::make_shared<f2fs::F2fsFileSystem>();
        NtStatus st = f2fsFs_->mount(device);
        if (!NT_SUCCESS(st)) {
            f2fsFs_.reset();
            return st;
        }
        return NtStatus::Success;
    }

    void setMountedF2fs(std::shared_ptr<f2fs::F2fsFileSystem> f2fs) {
        std::lock_guard<std::mutex> lock(mutex_);
        f2fsFs_ = f2fs;
    }

    [[nodiscard]] std::shared_ptr<f2fs::F2fsFileSystem> getMountedF2fs() const noexcept {
        return f2fsFs_;
    }

    NtStatus queryVolumeInfo(wchar_t driveLetter, FileSystemVolumeInfo& outInfo) const {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(driveLetter));
        auto it = driveMappings_.find(d);
        if (it == driveMappings_.end()) {
            return NtStatus::NoSuchDevice;
        }

        const auto& map = it->second;
        switch (map.type) {
            case PartitionResolution::VolumeType::Fat32:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_SYS" : map.volumeLabel,
                    .fileSystemName = L"FAT32",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0001-0000-0000-000000000001}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x1337B007,
                    .bytesPerSector = 512,
                    .sectorsPerCluster = 8,
                    .totalClusters = 32768000,
                    .freeClusters = 32760000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Ntfs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_NTFS" : map.volumeLabel,
                    .fileSystemName = L"NTFS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0002-0000-0000-000000000002}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x78E2A1C9,
                    .bytesPerSector = 512,
                    .sectorsPerCluster = 8,
                    .totalClusters = 32768000,
                    .freeClusters = 32736000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_FILE_COMPRESSION | FS_SUPPORTS_SPARSE_FILES | FS_SUPPORTS_REPARSE_POINTS | FS_SUPPORTS_OBJECT_IDS | FS_SUPPORTS_ENCRYPTION | FS_SUPPORTS_TRANSACTIONS | FS_SUPPORTS_USN_JOURNAL,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Exfat:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_EXFAT" : map.volumeLabel,
                    .fileSystemName = L"exFAT",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0003-0000-0000-000000000003}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x6A129F04,
                    .bytesPerSector = 512,
                    .sectorsPerCluster = 8,
                    .totalClusters = 32768000,
                    .freeClusters = 32767000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Cdfs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_CDFS" : map.volumeLabel,
                    .fileSystemName = L"CDFS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0004-0000-0000-000000000004}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0xC0DE1509,
                    .bytesPerSector = 2048,
                    .sectorsPerCluster = 1,
                    .totalClusters = 358400,
                    .freeClusters = 0,
                    .maxComponentLength = 110,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_READ_ONLY_VOLUME,
                    .isReadOnly = true,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Refs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_REFS" : map.volumeLabel,
                    .fileSystemName = L"ReFS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0005-0000-0000-000000000005}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x8E21F4A0,
                    .bytesPerSector = 512,
                    .sectorsPerCluster = 8,
                    .totalClusters = 32768000,
                    .freeClusters = 32764000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_INTEGRITY_STREAMS | FS_SUPPORTS_BLOCK_REFCOUNTING,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Zfs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_ZFS" : map.volumeLabel,
                    .fileSystemName = L"ZFS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0006-0000-0000-000000000006}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x00BAB10C,
                    .bytesPerSector = 4096,
                    .sectorsPerCluster = 1,
                    .totalClusters = 32768000,
                    .freeClusters = 32764000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_INTEGRITY_STREAMS | FS_SUPPORTS_COW_SNAPSHOTS | FS_SUPPORTS_RAID_PARITY,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Btrfs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_BTRFS" : map.volumeLabel,
                    .fileSystemName = L"Btrfs",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0007-0000-0000-000000000007}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0xB78F5C01,
                    .bytesPerSector = 4096,
                    .sectorsPerCluster = 1,
                    .totalClusters = 32768000,
                    .freeClusters = 32764000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_INTEGRITY_STREAMS | FS_SUPPORTS_COW_SNAPSHOTS | FS_SUPPORTS_RAID_PARITY,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Ext4:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_EXT4" : map.volumeLabel,
                    .fileSystemName = L"EXT4",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0008-0000-0000-000000000008}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0xEF534800,
                    .bytesPerSector = 4096,
                    .sectorsPerCluster = 1,
                    .totalClusters = 32768000,
                    .freeClusters = 32736000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_SPARSE_FILES,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::Xfs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_XFS" : map.volumeLabel,
                    .fileSystemName = L"XFS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0009-0000-0000-000000000009}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0x58465342,
                    .bytesPerSector = 4096,
                    .sectorsPerCluster = 1,
                    .totalClusters = 32768000,
                    .freeClusters = 32760000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_SPARSE_FILES | FS_SUPPORTS_INTEGRITY_STREAMS,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;

            case PartitionResolution::VolumeType::F2fs:
                outInfo = FileSystemVolumeInfo{
                    .driveLetter = d,
                    .volumeLabel = map.volumeLabel.empty() ? L"MICA_F2FS" : map.volumeLabel,
                    .fileSystemName = L"F2FS",
                    .volumeGuid = map.volumeGuid.empty() ? L"\\\\?\\Volume{a4bf011e-0010-0000-0000-000000000010}\\" : map.volumeGuid,
                    .devicePath = map.targetDevice,
                    .serialNumber = 0xF2F52026,
                    .bytesPerSector = 4096,
                    .sectorsPerCluster = 1,
                    .totalClusters = 32768000,
                    .freeClusters = 32762000,
                    .maxComponentLength = 255,
                    .fileSystemAttributes = FS_CASE_IS_PRESERVED | FS_UNICODE_STORED_ON_DISK | FS_PERSISTENT_ACLS | FS_SUPPORTS_SPARSE_FILES | FS_SUPPORTS_INTEGRITY_STREAMS,
                    .isReadOnly = false,
                    .isDirty = false
                };
                return NtStatus::Success;
        }
        return NtStatus::NoSuchDevice;
    }

    std::vector<FileSystemVolumeInfo> queryAllVolumes() const {
        std::vector<FileSystemVolumeInfo> vols;
        auto letters = getMountedDriveLetters();
        for (wchar_t l : letters) {
            FileSystemVolumeInfo vi;
            if (NT_SUCCESS(queryVolumeInfo(l, vi))) {
                vols.push_back(vi);
            }
        }
        return vols;
    }

    NtStatus createVolumeSnapshot(wchar_t driveLetter, std::string_view snapName) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(driveLetter));
        std::wstring volPath = std::wstring(1, d) + L":\\";
        auto pr = resolvePartitionPath(volPath);

        switch (pr.type) {
            case PartitionResolution::VolumeType::Btrfs:
                if (btrfsFs_) return btrfsFs_->createSnapshot(snapName);
                break;
            case PartitionResolution::VolumeType::Zfs:
                if (zfsFs_) return zfsFs_->createSnapshot(snapName);
                break;
            case PartitionResolution::VolumeType::Refs:
                if (refsFs_) return refsFs_->createCheckpoint(snapName);
                break;
            case PartitionResolution::VolumeType::Ntfs:
                ntfsSnapshots_[std::string(snapName)] = d;
                return NtStatus::Success;
            default:
                break;
        }
        return NtStatus::Success;
    }

    NtStatus rollbackVolumeSnapshot(wchar_t driveLetter, std::string_view snapName) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(driveLetter));
        std::wstring volPath = std::wstring(1, d) + L":\\";
        auto pr = resolvePartitionPath(volPath);

        switch (pr.type) {
            case PartitionResolution::VolumeType::Btrfs:
                if (btrfsFs_) return btrfsFs_->rollbackSnapshot(snapName);
                break;
            case PartitionResolution::VolumeType::Zfs:
                if (zfsFs_) return zfsFs_->rollbackSnapshot(snapName);
                break;
            case PartitionResolution::VolumeType::Refs:
                if (refsFs_) return refsFs_->rollbackCheckpoint(snapName);
                break;
            case PartitionResolution::VolumeType::Ntfs:
                if (ntfsSnapshots_.contains(std::string(snapName))) return NtStatus::Success;
                return NtStatus::NoSuchFile;
            default:
                break;
        }
        return NtStatus::Success;
    }

    [[nodiscard]] std::vector<std::string> listVolumeSnapshots(wchar_t driveLetter) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(driveLetter));
        std::wstring volPath = std::wstring(1, d) + L":\\";
        auto pr = resolvePartitionPath(volPath);

        switch (pr.type) {
            case PartitionResolution::VolumeType::Btrfs:
                if (btrfsFs_) return btrfsFs_->listSnapshots();
                break;
            case PartitionResolution::VolumeType::Zfs:
                if (zfsFs_) return zfsFs_->listSnapshots();
                break;
            case PartitionResolution::VolumeType::Refs:
                if (refsFs_) return refsFs_->listCheckpoints();
                break;
            case PartitionResolution::VolumeType::Ntfs: {
                std::vector<std::string> res;
                for (const auto& [name, drv] : ntfsSnapshots_) {
                    if (drv == d) res.push_back(name);
                }
                return res;
            }
            default:
                break;
        }
        return {};
    }

    NtStatus deleteVolumeSnapshot(wchar_t driveLetter, std::string_view snapName) {
        std::lock_guard<std::mutex> lock(mutex_);
        wchar_t d = static_cast<wchar_t>(std::towupper(driveLetter));
        std::wstring volPath = std::wstring(1, d) + L":\\";
        auto pr = resolvePartitionPath(volPath);

        switch (pr.type) {
            case PartitionResolution::VolumeType::Btrfs:
                if (btrfsFs_) return btrfsFs_->deleteSnapshot(snapName);
                break;
            case PartitionResolution::VolumeType::Zfs:
                return NtStatus::Success;
            case PartitionResolution::VolumeType::Refs:
                if (refsFs_) return refsFs_->deleteCheckpoint(snapName);
                break;
            case PartitionResolution::VolumeType::Ntfs:
                ntfsSnapshots_.erase(std::string(snapName));
                return NtStatus::Success;
            default:
                break;
        }
        return NtStatus::Success;
    }


    NtStatus createOrOpenFile(
        std::wstring_view path,
        uint32_t desiredAccess,
        uint32_t disposition,
        std::shared_ptr<FileObject>& outFileObj
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 0. Check for Named Pipe path
        if (path.starts_with(L"\\\\.\\pipe\\") ||
            path.starts_with(L"\\??\\pipe\\") ||
            path.starts_with(L"\\DosDevices\\pipe\\") ||
            path.starts_with(L"\\Device\\NamedPipe\\") ||
            path.starts_with(L"pipe\\")) {
            
            std::shared_ptr<npfs::NamedPipeInstance> pipeInst;
            NtStatus st = npfs::NamedPipeFileSystem::get().openClientPipe(
                path, desiredAccess, 0, pipeInst
            );
            if (!NT_SUCCESS(st)) return st;

            outFileObj = std::make_shared<FileObject>(
                npfs::NamedPipeFileSystem::get().getDevice(),
                path, desiredAccess
            );
            outFileObj->setFsContext(pipeInst.get());
            outFileObj->setFsContext2(reinterpret_cast<void*>(static_cast<uintptr_t>(0))); // Client end
            openFiles_[outFileObj.get()] = nullptr;
            openPipeInstances_[outFileObj.get()] = pipeInst;
            return NtStatus::Success;
        }

        // 0.1 Check for Mailslot path
        if (path.starts_with(L"\\\\.\\mailslot\\") ||
            path.starts_with(L"\\??\\mailslot\\") ||
            path.starts_with(L"\\DosDevices\\mailslot\\") ||
            path.starts_with(L"\\Device\\Mailslot\\") ||
            path.starts_with(L"mailslot\\")) {

            auto slot = npfs::MailslotFileSystem::get().lookupMailslot(path);
            if (!slot) return NtStatus::MailslotNotFound;

            outFileObj = std::make_shared<FileObject>(
                npfs::MailslotFileSystem::get().getDevice(),
                path, desiredAccess
            );
            outFileObj->setFsContext(slot.get());
            outFileObj->setFsContext2(reinterpret_cast<void*>(static_cast<uintptr_t>(0))); // Client writer
            openFiles_[outFileObj.get()] = nullptr;
            openMailslots_[outFileObj.get()] = slot;
            return NtStatus::Success;
        }

        // 0.2 Multi-Format Dynamic Partition Open/Create
        std::wstring sPath(path);
        std::wstring streamName;
        size_t colonPos = sPath.find(L':', 3); // Skip drive letter e.g. C: or D:
        if (colonPos != std::wstring::npos) {
            streamName = sPath.substr(colonPos + 1);
            sPath = sPath.substr(0, colonPos);
        }

        auto pr = resolvePartitionPath(sPath);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Ntfs: {
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t recNum = 0;
                NtStatus lookupSt = ntfsFs_->lookupPath(pr.relPath, recNum);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = ntfsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, recNum);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        ntfsFs_->truncateFile(recNum, streamName, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openNtfsFiles_[outFileObj.get()] = { recNum, streamName };
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Exfat: {
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                uint32_t firstClus = 0;
                NtStatus lookupSt = exfatFs_->lookupPath(pr.relPath, firstClus);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = exfatFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, firstClus);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        exfatFs_->truncateFile(pr.relPath, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openExfatFiles_[outFileObj.get()] = pr.relPath;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Cdfs: {
                if (!cdfsFs_) return NtStatus::VolumeNotMounted;
                if ((desiredAccess & FILE_WRITE_DATA) || disposition == FILE_CREATE || disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                    return NtStatus::MediaWriteProtected;
                }
                cdfs::CdfsFileEntry entry;
                NtStatus lookupSt = cdfsFs_->lookupPath(pr.relPath, entry);
                if (!NT_SUCCESS(lookupSt)) {
                    return NtStatus::NoSuchFile;
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openCdfsFiles_[outFileObj.get()] = pr.relPath;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Refs: {
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                uint64_t frn = 0;
                NtStatus lookupSt = refsFs_->lookupPath(pr.relPath, frn);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = refsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, frn);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        refsFs_->truncateFile(frn, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openRefsFiles_[outFileObj.get()] = frn;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Zfs: {
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                zfs::ZfsFileRecord zRec{};
                bool exists = zfsFs_->lookupPath(pr.relPath, zRec);
                if (!exists) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = zfsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, false);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        zfsFs_->truncateFile(pr.relPath, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openZfsFiles_[outFileObj.get()] = pr.relPath;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Btrfs: {
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                NtStatus lookupSt = btrfsFs_->lookupPath(pr.relPath, ino);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = btrfsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, ino);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        btrfsFs_->truncateFile(ino, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openBtrfsFiles_[outFileObj.get()] = ino;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Ext4: {
                if (!extFs_) return NtStatus::VolumeNotMounted;
                uint32_t ino = 0;
                NtStatus lookupSt = extFs_->lookupPath(pr.relPath, ino);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = extFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, ino);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        extFs_->truncateFile(ino, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openExtFiles_[outFileObj.get()] = ino;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Xfs: {
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                NtStatus lookupSt = xfsFs_->lookupPath(pr.relPath, ino);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = xfsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, ino);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        xfsFs_->truncateFile(ino, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openXfsFiles_[outFileObj.get()] = ino;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::F2fs: {
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                NtStatus lookupSt = f2fsFs_->lookupPath(pr.relPath, ino);
                if (!NT_SUCCESS(lookupSt)) {
                    if (disposition == FILE_CREATE || disposition == FILE_OPEN_IF || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        NtStatus crSt = f2fsFs_->createFile(pr.relPath, FILE_ATTRIBUTE_NORMAL, ino);
                        if (!NT_SUCCESS(crSt)) return crSt;
                    } else {
                        return NtStatus::NoSuchFile;
                    }
                } else {
                    if (disposition == FILE_CREATE) {
                        return NtStatus::ObjectNameCollision;
                    }
                    if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                        f2fsFs_->truncateFile(ino, 0);
                    }
                }
                outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), path, desiredAccess);
                openF2fsFiles_[outFileObj.get()] = ino;
                return NtStatus::Success;
            }

            case PartitionResolution::VolumeType::Fat32:
            default:
                break;
        }

        // 1. Check if path targets a registered device node in \Device
        std::wstring resolvedPath = ob::ObjectNamespace::get().resolvePath(path);
        auto directDevice = io::IoManager::get().lookupDevice(resolvedPath);
        if (!directDevice) {
            directDevice = io::IoManager::get().lookupDevice(path);
        }
        if (directDevice) {
            outFileObj = std::make_shared<FileObject>(directDevice.get(), path, desiredAccess);
            openFiles_[outFileObj.get()] = nullptr; // Device handle
            
            io::Irp createIrp{};
            createIrp.majorFunction = io::IRP_MJ_CREATE;
            createIrp.deviceObject = directDevice.get();
            return io::IoCallDriver(directDevice.get(), &createIrp);
        }

        std::wstring normalized = normalizePath(path);

        auto entry = findEntry(normalized);
        if (entry) {
            // File exists
            if (disposition == FILE_CREATE) {
                return NtStatus::ObjectNameCollision;
            }
            if (disposition == FILE_OVERWRITE || disposition == FILE_OVERWRITE_IF || disposition == FILE_SUPERSEDE) {
                entry->content.clear();
            }

            outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), normalized, desiredAccess);
            outFileObj->setDirectory(entry->isDirectory);
            outFileObj->getData() = entry->content;
            openFiles_[outFileObj.get()] = entry;
            return NtStatus::Success;
        }

        // File does not exist
        if (disposition == FILE_OPEN || disposition == FILE_OVERWRITE) {
            return NtStatus::NoSuchFile;
        }

        // Create new file
        auto newEntry = createPathEntries(normalized, false);
        if (!newEntry) {
            return NtStatus::ObjectPathNotFound;
        }

        outFileObj = std::make_shared<FileObject>(partitionDevice_.get(), normalized, desiredAccess);
        openFiles_[outFileObj.get()] = newEntry;
        return NtStatus::Success;
    }

    NtStatus readFile(
        FileObject* fileObj,
        void* buffer,
        uint32_t length,
        LargeInteger* byteOffset,
        uint32_t& bytesRead
    ) {
        if (!fileObj || !buffer) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);

        if (fileObj->getFsContext()) {
            auto itP = openPipeInstances_.find(fileObj);
            if (itP != openPipeInstances_.end() && itP->second) {
                bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);
                return itP->second->read(isServer, buffer, length, bytesRead);
            }
            auto itM = openMailslots_.find(fileObj);
            if (itM != openMailslots_.end() && itM->second) {
                return itM->second->read(buffer, length, bytesRead, itM->second->getReadTimeout());
            }
        }

        auto itNtfs = openNtfsFiles_.find(fileObj);
        if (itNtfs != openNtfsFiles_.end() && ntfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t ntfsBytes = 0;
            NtStatus st = ntfsFs_->readFile(itNtfs->second.first, itNtfs->second.second, buffer, length, static_cast<uint64_t>(offset), ntfsBytes);
            bytesRead = static_cast<uint32_t>(ntfsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itExfat = openExfatFiles_.find(fileObj);
        if (itExfat != openExfatFiles_.end() && exfatFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t exBytes = 0;
            NtStatus st = exfatFs_->readFile(itExfat->second, buffer, length, static_cast<uint64_t>(offset), exBytes);
            bytesRead = static_cast<uint32_t>(exBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itCdfs = openCdfsFiles_.find(fileObj);
        if (itCdfs != openCdfsFiles_.end() && cdfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t cdfsBytes = 0;
            NtStatus st = cdfsFs_->readFile(itCdfs->second, buffer, length, static_cast<uint64_t>(offset), cdfsBytes);
            bytesRead = static_cast<uint32_t>(cdfsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itRefs = openRefsFiles_.find(fileObj);
        if (itRefs != openRefsFiles_.end() && refsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t refsBytes = 0;
            NtStatus st = refsFs_->readFile(itRefs->second, buffer, length, static_cast<uint64_t>(offset), refsBytes);
            bytesRead = static_cast<uint32_t>(refsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itZfs = openZfsFiles_.find(fileObj);
        if (itZfs != openZfsFiles_.end() && zfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t zfsBytes = 0;
            NtStatus st = zfsFs_->readFile(itZfs->second, buffer, length, static_cast<uint64_t>(offset), zfsBytes);
            bytesRead = static_cast<uint32_t>(zfsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itBtrfs = openBtrfsFiles_.find(fileObj);
        if (itBtrfs != openBtrfsFiles_.end() && btrfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t bBytes = 0;
            NtStatus st = btrfsFs_->readFile(itBtrfs->second, buffer, length, static_cast<uint64_t>(offset), bBytes);
            bytesRead = static_cast<uint32_t>(bBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itExt = openExtFiles_.find(fileObj);
        if (itExt != openExtFiles_.end() && extFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t eBytes = 0;
            NtStatus st = extFs_->readFile(itExt->second, buffer, length, static_cast<uint64_t>(offset), eBytes);
            bytesRead = static_cast<uint32_t>(eBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itXfs = openXfsFiles_.find(fileObj);
        if (itXfs != openXfsFiles_.end() && xfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t xBytes = 0;
            NtStatus st = xfsFs_->readFile(itXfs->second, buffer, length, static_cast<uint64_t>(offset), xBytes);
            bytesRead = static_cast<uint32_t>(xBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto itF2fs = openF2fsFiles_.find(fileObj);
        if (itF2fs != openF2fsFiles_.end() && f2fsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t fBytes = 0;
            NtStatus st = f2fsFs_->readFile(itF2fs->second, buffer, length, static_cast<uint64_t>(offset), fBytes);
            bytesRead = static_cast<uint32_t>(fBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        // Direct device I/O (e.g. \Device\Harddisk0, \Device\Null)
        if (fileObj->getDeviceObject() && fileObj->getDeviceObject() != partitionDevice_.get()) {
            io::Irp readIrp{};
            readIrp.majorFunction = io::IRP_MJ_READ;
            readIrp.deviceObject = fileObj->getDeviceObject();
            readIrp.userBuffer = buffer;
            readIrp.length = length;
            if (byteOffset) {
                readIrp.byteOffset = *byteOffset;
            } else {
                readIrp.byteOffset.quadPart = fileObj->getCurrentByteOffset();
            }
            NtStatus st = io::IoCallDriver(fileObj->getDeviceObject(), &readIrp);
            bytesRead = readIrp.ioStatus.information;
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesRead);
            }
            return st;
        }

        auto it = openFiles_.find(fileObj);
        if (it == openFiles_.end() || !it->second) return NtStatus::InvalidHandle;

        auto entry = it->second;
        int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
        if (offset < 0 || static_cast<size_t>(offset) >= entry->content.size()) {
            bytesRead = 0;
            return (entry->content.empty() && offset == 0) ? NtStatus::Success : NtStatus::EndOfFile;
        }

        size_t available = entry->content.size() - static_cast<size_t>(offset);
        size_t toRead = std::min<size_t>(length, available);
        std::memcpy(buffer, entry->content.data() + offset, toRead);
        bytesRead = static_cast<uint32_t>(toRead);

        if (!byteOffset) {
            fileObj->advanceByteOffset(toRead);
        }

        return NtStatus::Success;
    }

    NtStatus writeFile(
        FileObject* fileObj,
        const void* buffer,
        uint32_t length,
        LargeInteger* byteOffset,
        uint32_t& bytesWritten
    ) {
        if (!fileObj || !buffer) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(mutex_);

        if (fileObj->getFsContext()) {
            auto itP = openPipeInstances_.find(fileObj);
            if (itP != openPipeInstances_.end() && itP->second) {
                bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);
                return itP->second->write(isServer, buffer, length, bytesWritten);
            }
            auto itM = openMailslots_.find(fileObj);
            if (itM != openMailslots_.end() && itM->second) {
                bytesWritten = length;
                return itM->second->write(buffer, length);
            }
        }

        auto itNtfs = openNtfsFiles_.find(fileObj);
        if (itNtfs != openNtfsFiles_.end() && ntfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t ntfsBytes = 0;
            NtStatus st = ntfsFs_->writeFile(itNtfs->second.first, itNtfs->second.second, buffer, length, static_cast<uint64_t>(offset), ntfsBytes);
            bytesWritten = static_cast<uint32_t>(ntfsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itExfat = openExfatFiles_.find(fileObj);
        if (itExfat != openExfatFiles_.end() && exfatFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t exBytes = 0;
            NtStatus st = exfatFs_->writeFile(itExfat->second, buffer, length, static_cast<uint64_t>(offset), exBytes);
            bytesWritten = static_cast<uint32_t>(exBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itCdfs = openCdfsFiles_.find(fileObj);
        if (itCdfs != openCdfsFiles_.end() && cdfsFs_) {
            bytesWritten = 0;
            return NtStatus::MediaWriteProtected;
        }

        auto itRefs = openRefsFiles_.find(fileObj);
        if (itRefs != openRefsFiles_.end() && refsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t refsBytes = 0;
            NtStatus st = refsFs_->writeFile(itRefs->second, buffer, length, static_cast<uint64_t>(offset), refsBytes);
            bytesWritten = static_cast<uint32_t>(refsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itZfs = openZfsFiles_.find(fileObj);
        if (itZfs != openZfsFiles_.end() && zfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t zfsBytes = 0;
            NtStatus st = zfsFs_->writeFile(itZfs->second, buffer, length, static_cast<uint64_t>(offset), zfsBytes);
            bytesWritten = static_cast<uint32_t>(zfsBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itBtrfs = openBtrfsFiles_.find(fileObj);
        if (itBtrfs != openBtrfsFiles_.end() && btrfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t bBytes = 0;
            NtStatus st = btrfsFs_->writeFile(itBtrfs->second, buffer, length, static_cast<uint64_t>(offset), bBytes);
            bytesWritten = static_cast<uint32_t>(bBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itExt = openExtFiles_.find(fileObj);
        if (itExt != openExtFiles_.end() && extFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t eBytes = 0;
            NtStatus st = extFs_->writeFile(itExt->second, buffer, length, static_cast<uint64_t>(offset), eBytes);
            bytesWritten = static_cast<uint32_t>(eBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itXfs = openXfsFiles_.find(fileObj);
        if (itXfs != openXfsFiles_.end() && xfsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t xBytes = 0;
            NtStatus st = xfsFs_->writeFile(itXfs->second, buffer, length, static_cast<uint64_t>(offset), xBytes);
            bytesWritten = static_cast<uint32_t>(xBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto itF2fs = openF2fsFiles_.find(fileObj);
        if (itF2fs != openF2fsFiles_.end() && f2fsFs_) {
            int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
            if (offset < 0) offset = 0;
            uint64_t fBytes = 0;
            NtStatus st = f2fsFs_->writeFile(itF2fs->second, buffer, length, static_cast<uint64_t>(offset), fBytes);
            bytesWritten = static_cast<uint32_t>(fBytes);
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        // Direct device I/O (e.g. \Device\Harddisk0, \Device\Null)
        if (fileObj->getDeviceObject() && fileObj->getDeviceObject() != partitionDevice_.get()) {
            io::Irp writeIrp{};
            writeIrp.majorFunction = io::IRP_MJ_WRITE;
            writeIrp.deviceObject = fileObj->getDeviceObject();
            writeIrp.userBuffer = const_cast<void*>(buffer);
            writeIrp.length = length;
            if (byteOffset) {
                writeIrp.byteOffset = *byteOffset;
            } else {
                writeIrp.byteOffset.quadPart = fileObj->getCurrentByteOffset();
            }
            NtStatus st = io::IoCallDriver(fileObj->getDeviceObject(), &writeIrp);
            bytesWritten = writeIrp.ioStatus.information;
            if (NT_SUCCESS(st) && !byteOffset) {
                fileObj->advanceByteOffset(bytesWritten);
            }
            return st;
        }

        auto it = openFiles_.find(fileObj);
        if (it == openFiles_.end() || !it->second) return NtStatus::InvalidHandle;

        auto entry = it->second;
        int64_t offset = byteOffset ? byteOffset->quadPart : fileObj->getCurrentByteOffset();
        if (offset < 0) offset = 0;

        size_t requiredSize = static_cast<size_t>(offset) + length;
        if (requiredSize > entry->content.size()) {
            entry->content.resize(requiredSize);
        }

        std::memcpy(entry->content.data() + offset, buffer, length);
        bytesWritten = length;

        if (!byteOffset) {
            fileObj->advanceByteOffset(length);
        }

        // Synchronize in-memory file object data
        fileObj->getData() = entry->content;
        return NtStatus::Success;
    }

    void closeFile(FileObject* fileObj) {
        if (!fileObj) return;
        std::lock_guard<std::mutex> lock(mutex_);
        auto itP = openPipeInstances_.find(fileObj);
        if (itP != openPipeInstances_.end()) {
            if (itP->second) {
                bool isServer = (reinterpret_cast<uintptr_t>(fileObj->getFsContext2()) == 1);
                if (isServer) {
                    itP->second->disconnectServer();
                } else {
                    itP->second->clientClose();
                }
            }
            openPipeInstances_.erase(itP);
        }
        openMailslots_.erase(fileObj);
        openNtfsFiles_.erase(fileObj);
        openExfatFiles_.erase(fileObj);
        openCdfsFiles_.erase(fileObj);
        openRefsFiles_.erase(fileObj);
        openZfsFiles_.erase(fileObj);
        openBtrfsFiles_.erase(fileObj);
        openExtFiles_.erase(fileObj);
        openXfsFiles_.erase(fileObj);
        openF2fsFiles_.erase(fileObj);
        openFiles_.erase(fileObj);
    }

    std::shared_ptr<FileObject> registerServerPipe(
        std::shared_ptr<npfs::NamedPipeInstance> pipeInst,
        uint32_t desiredAccess
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto fileObj = std::make_shared<FileObject>(
            npfs::NamedPipeFileSystem::get().getDevice(),
            pipeInst->getName(), desiredAccess
        );
        fileObj->setFsContext(pipeInst.get());
        fileObj->setFsContext2(reinterpret_cast<void*>(static_cast<uintptr_t>(1))); // Server end
        openFiles_[fileObj.get()] = nullptr;
        openPipeInstances_[fileObj.get()] = pipeInst;
        return fileObj;
    }

    std::shared_ptr<FileObject> registerServerMailslot(
        std::shared_ptr<npfs::Mailslot> slot
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto fileObj = std::make_shared<FileObject>(
            npfs::MailslotFileSystem::get().getDevice(),
            slot->getName(), FILE_GENERIC_READ
        );
        fileObj->setFsContext(slot.get());
        fileObj->setFsContext2(reinterpret_cast<void*>(static_cast<uintptr_t>(1))); // Server reader
        openFiles_[fileObj.get()] = nullptr;
        openMailslots_[fileObj.get()] = slot;
        return fileObj;
    }

    std::shared_ptr<npfs::NamedPipeInstance> getPipeInstance(FileObject* fileObj) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = openPipeInstances_.find(fileObj);
        if (it != openPipeInstances_.end()) return it->second;
        return nullptr;
    }

    std::shared_ptr<npfs::Mailslot> getMailslot(FileObject* fileObj) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = openMailslots_.find(fileObj);
        if (it != openMailslots_.end()) return it->second;
        return nullptr;
    }

    struct DirectoryEntry {
        std::wstring name;
        uint32_t attributes{0};
        size_t size{0};
        bool isDirectory{false};
    };

    PartitionResolution resolvePartitionPath(std::wstring_view rawPath) const {
        std::wstring s(rawPath);
        for (auto& c : s) {
            if (c == L'/') c = L'\\';
        }
        if (s.starts_with(L"\\DosDevices\\")) s = s.substr(12);
        else if (s.starts_with(L"\\??\\")) s = s.substr(4);

        PartitionResolution pr;
        if (s.starts_with(L"\\Device\\Btrfs0\\") || s.starts_with(L"\\Device\\Harddisk0\\PartitionBtrfs\\") || s.starts_with(L"\\Device\\Btrfs\\")) {
            pr.type = PartitionResolution::VolumeType::Btrfs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\Btrfs0" || s == L"\\Device\\Harddisk0\\PartitionBtrfs" || s == L"\\Device\\Btrfs") {
            pr.type = PartitionResolution::VolumeType::Btrfs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\ExtFs0\\") || s.starts_with(L"\\Device\\Harddisk0\\PartitionExt4\\") || s.starts_with(L"\\Device\\ExtFs\\")) {
            pr.type = PartitionResolution::VolumeType::Ext4;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\ExtFs0" || s == L"\\Device\\Harddisk0\\PartitionExt4" || s == L"\\Device\\ExtFs") {
            pr.type = PartitionResolution::VolumeType::Ext4;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\Xfs0\\") || s.starts_with(L"\\Device\\Harddisk0\\PartitionXfs\\") || s.starts_with(L"\\Device\\Xfs\\")) {
            pr.type = PartitionResolution::VolumeType::Xfs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\Xfs0" || s == L"\\Device\\Harddisk0\\PartitionXfs" || s == L"\\Device\\Xfs") {
            pr.type = PartitionResolution::VolumeType::Xfs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\F2fs0\\") || s.starts_with(L"\\Device\\Harddisk0\\PartitionF2fs\\") || s.starts_with(L"\\Device\\F2fs\\")) {
            pr.type = PartitionResolution::VolumeType::F2fs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\F2fs0" || s == L"\\Device\\Harddisk0\\PartitionF2fs" || s == L"\\Device\\F2fs") {
            pr.type = PartitionResolution::VolumeType::F2fs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\ZfsPool_Storage\\") || s.starts_with(L"\\Device\\Zfs0\\")) {
            pr.type = PartitionResolution::VolumeType::Zfs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\ZfsPool_Storage" || s == L"\\Device\\Zfs0") {
            pr.type = PartitionResolution::VolumeType::Zfs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\Harddisk0\\Partition2\\")) {
            pr.type = PartitionResolution::VolumeType::Ntfs;
            pr.relPath = s.substr(29);
        } else if (s == L"\\Device\\Harddisk0\\Partition2") {
            pr.type = PartitionResolution::VolumeType::Ntfs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\Harddisk0\\Partition3\\") || s.starts_with(L"\\Device\\ExFat0\\") || s.starts_with(L"\\Device\\ExFat_E\\")) {
            pr.type = PartitionResolution::VolumeType::Exfat;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\Harddisk0\\Partition3" || s == L"\\Device\\ExFat0" || s == L"\\Device\\ExFat_E") {
            pr.type = PartitionResolution::VolumeType::Exfat;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\CdRom0\\") || s.starts_with(L"\\Device\\CdRom_F\\")) {
            pr.type = PartitionResolution::VolumeType::Cdfs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\CdRom0" || s == L"\\Device\\CdRom_F") {
            pr.type = PartitionResolution::VolumeType::Cdfs;
            pr.relPath = L"";
        } else if (s.starts_with(L"\\Device\\Harddisk0\\Partition4\\") || s.starts_with(L"\\Device\\ReFS0\\") || s.starts_with(L"\\Device\\ReFS_G\\")) {
            pr.type = PartitionResolution::VolumeType::Refs;
            size_t devEnd = s.find(L'\\', 8);
            pr.relPath = (devEnd != std::wstring::npos) ? s.substr(devEnd + 1) : L"";
        } else if (s == L"\\Device\\Harddisk0\\Partition4" || s == L"\\Device\\ReFS0" || s == L"\\Device\\ReFS_G") {
            pr.type = PartitionResolution::VolumeType::Refs;
            pr.relPath = L"";
        } else if (s.size() >= 2 && s[1] == L':') {
            wchar_t drive = static_cast<wchar_t>(std::towupper(s[0]));
            std::wstring rel = s.substr(2);
            while (rel.starts_with(L"\\")) rel = rel.substr(1);

            auto it = driveMappings_.find(drive);
            if (it != driveMappings_.end()) {
                pr.type = it->second.type;
            } else {
                auto sym = ob::ObjectNamespace::get().getSymbolicLink(std::wstring(L"\\DosDevices\\") + drive + L":");
                if (sym) {
                    const auto& tgt = sym->getTargetPath();
                    if (tgt.contains(L"Zfs") || tgt.contains(L"tank")) pr.type = PartitionResolution::VolumeType::Zfs;
                    else if (tgt.contains(L"Btrfs") || tgt.contains(L"btrfs")) pr.type = PartitionResolution::VolumeType::Btrfs;
                    else if (tgt.contains(L"Ext") || tgt.contains(L"ext")) pr.type = PartitionResolution::VolumeType::Ext4;
                    else if (tgt.contains(L"Xfs") || tgt.contains(L"xfs")) pr.type = PartitionResolution::VolumeType::Xfs;
                    else if (tgt.contains(L"F2fs") || tgt.contains(L"f2fs")) pr.type = PartitionResolution::VolumeType::F2fs;
                    else if (tgt.contains(L"Partition2") || tgt.contains(L"Ntfs")) pr.type = PartitionResolution::VolumeType::Ntfs;
                    else if (tgt.contains(L"Partition3") || tgt.contains(L"ExFat")) pr.type = PartitionResolution::VolumeType::Exfat;
                    else if (tgt.contains(L"CdRom") || tgt.contains(L"Cdfs")) pr.type = PartitionResolution::VolumeType::Cdfs;
                    else if (tgt.contains(L"Partition4") || tgt.contains(L"Refs")) pr.type = PartitionResolution::VolumeType::Refs;
                    else pr.type = PartitionResolution::VolumeType::Fat32;
                } else {
                    pr.type = PartitionResolution::VolumeType::Fat32;
                }
            }
            pr.relPath = std::move(rel);
        } else {
            while (s.starts_with(L"\\")) s = s.substr(1);
            pr.type = PartitionResolution::VolumeType::Fat32;
            pr.relPath = std::move(s);
        }

        // Clean up any trailing wildcard or slashes
        if (pr.relPath.ends_with(L"\\*")) {
            pr.relPath.resize(pr.relPath.size() - 2);
        } else if (pr.relPath == L"*") {
            pr.relPath.clear();
        } else if (pr.relPath.ends_with(L"*")) {
            pr.relPath.pop_back();
        }
        while (pr.relPath.ends_with(L"\\")) {
            pr.relPath.pop_back();
        }

        return pr;
    }

    static PartitionResolution resolvePartitionPathStatic(std::wstring_view rawPath) {
        return get().resolvePartitionPath(rawPath);
    }

    NtStatus createDirectory(std::wstring_view path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Cdfs:
                return NtStatus::MediaWriteProtected;
            case PartitionResolution::VolumeType::Zfs:
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                return zfsFs_->createFile(pr.relPath, 0x10, true);
            case PartitionResolution::VolumeType::Btrfs: {
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                return btrfsFs_->createFile(pr.relPath, 0x10, ino);
            }
            case PartitionResolution::VolumeType::Ext4: {
                if (!extFs_) return NtStatus::VolumeNotMounted;
                uint32_t ino = 0;
                return extFs_->createFile(pr.relPath, 0x10, ino);
            }
            case PartitionResolution::VolumeType::Xfs: {
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                return xfsFs_->createFile(pr.relPath, 0x10, ino);
            }
            case PartitionResolution::VolumeType::F2fs: {
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                uint64_t ino = 0;
                return f2fsFs_->createFile(pr.relPath, 0x10, ino);
            }
            case PartitionResolution::VolumeType::Ntfs: {
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                uint64_t rec = 0;
                return ntfsFs_->createDirectory(pr.relPath, rec);
            }
            case PartitionResolution::VolumeType::Exfat: {
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                uint32_t clus = 0;
                return exfatFs_->createDirectory(pr.relPath, clus);
            }
            case PartitionResolution::VolumeType::Refs: {
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                uint64_t frn = 0;
                return refsFs_->createFile(pr.relPath, 0x10, frn);
            }
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                if (findEntry(norm)) return NtStatus::ObjectNameCollision;
                auto entry = createPathEntries(norm, true);
                return entry ? NtStatus::Success : NtStatus::ObjectPathNotFound;
            }
        }
    }

    NtStatus deleteFile(std::wstring_view path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Cdfs:
                return NtStatus::MediaWriteProtected;
            case PartitionResolution::VolumeType::Zfs:
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                return zfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Btrfs:
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                return btrfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ext4:
                if (!extFs_) return NtStatus::VolumeNotMounted;
                return extFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Xfs:
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                return xfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::F2fs:
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                return f2fsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ntfs:
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                return ntfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Exfat:
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                return exfatFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Refs:
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                return refsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                size_t lastSlash = norm.find_last_of(L'\\');
                std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : norm.substr(0, lastSlash);
                std::wstring fileName = (lastSlash == std::wstring::npos) ? norm : norm.substr(lastSlash + 1);

                auto parent = findEntry(parentPath);
                if (!parent || !parent->isDirectory) return NtStatus::ObjectPathNotFound;

                auto it = parent->children.find(fileName);
                if (it == parent->children.end()) return NtStatus::NoSuchFile;
                if (it->second->isDirectory) return NtStatus::FileIsADirectory;

                parent->children.erase(it);
                return NtStatus::Success;
            }
        }
    }

    NtStatus removeDirectory(std::wstring_view path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Cdfs:
                return NtStatus::MediaWriteProtected;
            case PartitionResolution::VolumeType::Zfs:
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                return zfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Btrfs:
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                return btrfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ext4:
                if (!extFs_) return NtStatus::VolumeNotMounted;
                return extFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Xfs:
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                return xfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::F2fs:
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                return f2fsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ntfs:
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                return ntfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Exfat:
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                return exfatFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Refs:
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                return refsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                size_t lastSlash = norm.find_last_of(L'\\');
                std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : norm.substr(0, lastSlash);
                std::wstring dirName = (lastSlash == std::wstring::npos) ? norm : norm.substr(lastSlash + 1);

                auto parent = findEntry(parentPath);
                if (!parent || !parent->isDirectory) return NtStatus::ObjectPathNotFound;

                auto it = parent->children.find(dirName);
                if (it == parent->children.end()) return NtStatus::NoSuchFile;
                if (!it->second->isDirectory) return NtStatus::NotADirectory;
                if (!it->second->children.empty()) return NtStatus::DirectoryNotEmpty;

                parent->children.erase(it);
                return NtStatus::Success;
            }
        }
    }

    NtStatus removeDirectoryRecursive(std::wstring_view path) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Cdfs:
                return NtStatus::MediaWriteProtected;
            case PartitionResolution::VolumeType::Zfs:
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                return zfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Btrfs:
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                return btrfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ext4:
                if (!extFs_) return NtStatus::VolumeNotMounted;
                return extFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Xfs:
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                return xfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::F2fs:
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                return f2fsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Ntfs:
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                return ntfsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Exfat:
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                return exfatFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Refs:
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                return refsFs_->deleteFile(pr.relPath);
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                size_t lastSlash = norm.find_last_of(L'\\');
                std::wstring parentPath = (lastSlash == std::wstring::npos) ? L"" : norm.substr(0, lastSlash);
                std::wstring dirName = (lastSlash == std::wstring::npos) ? norm : norm.substr(lastSlash + 1);

                auto parent = findEntry(parentPath);
                if (!parent || !parent->isDirectory) return NtStatus::ObjectPathNotFound;

                auto it = parent->children.find(dirName);
                if (it == parent->children.end()) return NtStatus::NoSuchFile;
                if (!it->second->isDirectory) return NtStatus::NotADirectory;

                parent->children.erase(it);
                return NtStatus::Success;
            }
        }
    }

    NtStatus queryFileAttributes(std::wstring_view path, uint32_t& attributes) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        switch (pr.type) {
            case PartitionResolution::VolumeType::Zfs: {
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10; // FILE_ATTRIBUTE_DIRECTORY
                    return NtStatus::Success;
                }
                zfs::ZfsFileRecord zRec{};
                if (zfsFs_->lookupPath(pr.relPath, zRec)) {
                    attributes = zRec.attributes;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Btrfs: {
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint64_t ino = 0;
                if (NT_SUCCESS(btrfsFs_->lookupPath(pr.relPath, ino))) {
                    attributes = 0x80;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Ext4: {
                if (!extFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint32_t ino = 0;
                if (NT_SUCCESS(extFs_->lookupPath(pr.relPath, ino))) {
                    attributes = 0x80;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Xfs: {
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint64_t ino = 0;
                if (NT_SUCCESS(xfsFs_->lookupPath(pr.relPath, ino))) {
                    attributes = 0x80;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::F2fs: {
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint64_t ino = 0;
                if (NT_SUCCESS(f2fsFs_->lookupPath(pr.relPath, ino))) {
                    attributes = 0x80;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Ntfs: {
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint64_t recNum = 0;
                if (NT_SUCCESS(ntfsFs_->lookupPath(pr.relPath, recNum))) {
                    const auto* rec = ntfsFs_->getRecord(recNum);
                    if (rec) {
                        attributes = (rec->flags & ntfs::MFT_RECORD_DIRECTORY) ? 0x10u : 0x80u;
                        if (rec->stdInfo.dosPermissions) attributes |= rec->stdInfo.dosPermissions;
                        return NtStatus::Success;
                    }
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Exfat: {
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint32_t clus = 0;
                if (NT_SUCCESS(exfatFs_->lookupPath(pr.relPath, clus))) {
                    attributes = 0x80; // FILE_ATTRIBUTE_NORMAL
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Cdfs: {
                if (!cdfsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x11; // DIRECTORY | READONLY
                    return NtStatus::Success;
                }
                cdfs::CdfsFileEntry cEntry;
                if (NT_SUCCESS(cdfsFs_->lookupPath(pr.relPath, cEntry))) {
                    attributes = (cEntry.isDirectory ? 0x10u : 0x80u) | 0x01u; // READONLY
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Refs: {
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                if (pr.relPath.empty()) {
                    attributes = 0x10;
                    return NtStatus::Success;
                }
                uint64_t frn = 0;
                if (NT_SUCCESS(refsFs_->lookupPath(pr.relPath, frn))) {
                    attributes = 0x80;
                    return NtStatus::Success;
                }
                return NtStatus::NoSuchFile;
            }
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                auto entry = findEntry(norm);
                if (!entry) return NtStatus::NoSuchFile;
                attributes = entry->attributes;
                return NtStatus::Success;
            }
        }
    }

    NtStatus setFileAttributes(std::wstring_view path, uint32_t attributes) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        if (pr.type == PartitionResolution::VolumeType::Cdfs) {
            return NtStatus::MediaWriteProtected;
        }
        std::wstring norm = normalizePath(path);
        auto entry = findEntry(norm);
        if (!entry) return NtStatus::NoSuchFile;
        entry->attributes = attributes;
        return NtStatus::Success;
    }

    NtStatus queryDirectory(std::wstring_view path, std::vector<DirectoryEntry>& entries) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto pr = resolvePartitionPath(path);
        entries.clear();

        switch (pr.type) {
            case PartitionResolution::VolumeType::Zfs: {
                if (!zfsFs_) return NtStatus::VolumeNotMounted;
                auto files = zfsFs_->getAllFiles();
                for (const auto& f : files) {
                    if (pr.relPath.empty()) {
                        if (f.name.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = f.name,
                                .attributes = f.attributes,
                                .size = f.size,
                                .isDirectory = f.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (f.name.starts_with(prefix)) {
                            std::wstring sub = f.name.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = f.attributes,
                                    .size = f.size,
                                    .isDirectory = f.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Btrfs: {
                if (!btrfsFs_) return NtStatus::VolumeNotMounted;
                auto files = btrfsFs_->getAllFiles();
                for (const auto& f : files) {
                    if (f.fileName == L"\\") continue;
                    std::wstring fname = f.fileName;
                    while (fname.starts_with(L"\\")) fname = fname.substr(1);
                    if (pr.relPath.empty()) {
                        if (fname.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = fname,
                                .attributes = f.attributes,
                                .size = f.size,
                                .isDirectory = f.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (fname.starts_with(prefix)) {
                            std::wstring sub = fname.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = f.attributes,
                                    .size = f.size,
                                    .isDirectory = f.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Ext4: {
                if (!extFs_) return NtStatus::VolumeNotMounted;
                auto files = extFs_->getAllFiles();
                for (const auto& f : files) {
                    if (f.fileName == L"\\") continue;
                    std::wstring fname = f.fileName;
                    while (fname.starts_with(L"\\")) fname = fname.substr(1);
                    if (pr.relPath.empty()) {
                        if (fname.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = fname,
                                .attributes = f.attributes,
                                .size = f.size,
                                .isDirectory = f.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (fname.starts_with(prefix)) {
                            std::wstring sub = fname.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = f.attributes,
                                    .size = f.size,
                                    .isDirectory = f.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Xfs: {
                if (!xfsFs_) return NtStatus::VolumeNotMounted;
                auto files = xfsFs_->getAllFiles();
                for (const auto& f : files) {
                    if (f.fileName == L"\\") continue;
                    std::wstring fname = f.fileName;
                    while (fname.starts_with(L"\\")) fname = fname.substr(1);
                    if (pr.relPath.empty()) {
                        if (fname.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = fname,
                                .attributes = f.attributes,
                                .size = f.size,
                                .isDirectory = f.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (fname.starts_with(prefix)) {
                            std::wstring sub = fname.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = f.attributes,
                                    .size = f.size,
                                    .isDirectory = f.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::F2fs: {
                if (!f2fsFs_) return NtStatus::VolumeNotMounted;
                auto files = f2fsFs_->getAllFiles();
                for (const auto& f : files) {
                    if (f.fileName == L"\\") continue;
                    std::wstring fname = f.fileName;
                    while (fname.starts_with(L"\\")) fname = fname.substr(1);
                    if (pr.relPath.empty()) {
                        if (fname.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = fname,
                                .attributes = f.attributes,
                                .size = f.size,
                                .isDirectory = f.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (fname.starts_with(prefix)) {
                            std::wstring sub = fname.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = f.attributes,
                                    .size = f.size,
                                    .isDirectory = f.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Ntfs: {
                if (!ntfsFs_) return NtStatus::VolumeNotMounted;
                auto recs = ntfsFs_->getAllRecords();
                for (const auto& r : recs) {
                    if (pr.relPath.empty()) {
                        if (r.fileName.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = r.fileName,
                                .attributes = (r.flags & ntfs::MFT_RECORD_DIRECTORY) ? 0x10u : 0x80u,
                                .size = r.primaryData.size(),
                                .isDirectory = (r.flags & ntfs::MFT_RECORD_DIRECTORY) != 0
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (r.fileName.starts_with(prefix)) {
                            std::wstring sub = r.fileName.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = (r.flags & ntfs::MFT_RECORD_DIRECTORY) ? 0x10u : 0x80u,
                                    .size = r.primaryData.size(),
                                    .isDirectory = (r.flags & ntfs::MFT_RECORD_DIRECTORY) != 0
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Exfat: {
                if (!exfatFs_) return NtStatus::VolumeNotMounted;
                auto recs = exfatFs_->getAllRecords();
                for (const auto& r : recs) {
                    if (pr.relPath.empty()) {
                        if (r.fileName.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = r.fileName,
                                .attributes = r.attributes,
                                .size = r.data.size(),
                                .isDirectory = (r.attributes & 0x10) != 0
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (r.fileName.starts_with(prefix)) {
                            std::wstring sub = r.fileName.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = r.attributes,
                                    .size = r.data.size(),
                                    .isDirectory = (r.attributes & 0x10) != 0
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Cdfs: {
                if (!cdfsFs_) return NtStatus::VolumeNotMounted;
                auto recs = cdfsFs_->getAllRecords();
                for (const auto& r : recs) {
                    if (pr.relPath.empty()) {
                        if (r.fileName.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = r.fileName,
                                .attributes = (r.isDirectory ? 0x10u : 0x80u) | 0x01u,
                                .size = r.dataLength,
                                .isDirectory = r.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (r.fileName.starts_with(prefix)) {
                            std::wstring sub = r.fileName.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = (r.isDirectory ? 0x10u : 0x80u) | 0x01u,
                                    .size = r.dataLength,
                                    .isDirectory = r.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Refs: {
                if (!refsFs_) return NtStatus::VolumeNotMounted;
                auto recs = refsFs_->getAllRecords();
                for (const auto& r : recs) {
                    if (pr.relPath.empty()) {
                        if (r.fileName.find(L'\\') == std::wstring::npos) {
                            entries.push_back(DirectoryEntry{
                                .name = r.fileName,
                                .attributes = r.attributes,
                                .size = r.size,
                                .isDirectory = r.isDirectory
                            });
                        }
                    } else {
                        std::wstring prefix = pr.relPath + L"\\";
                        if (r.fileName.starts_with(prefix)) {
                            std::wstring sub = r.fileName.substr(prefix.size());
                            if (sub.find(L'\\') == std::wstring::npos) {
                                entries.push_back(DirectoryEntry{
                                    .name = sub,
                                    .attributes = r.attributes,
                                    .size = r.size,
                                    .isDirectory = r.isDirectory
                                });
                            }
                        }
                    }
                }
                return NtStatus::Success;
            }
            case PartitionResolution::VolumeType::Fat32:
            default: {
                std::wstring norm = normalizePath(path);
                auto entry = findEntry(norm);
                if (!entry) return NtStatus::NoSuchFile;
                if (!entry->isDirectory) return NtStatus::NotADirectory;

                for (const auto& [name, child] : entry->children) {
                    entries.push_back(DirectoryEntry{
                        .name = name,
                        .attributes = child->attributes,
                        .size = child->content.size(),
                        .isDirectory = child->isDirectory
                    });
                }
                return NtStatus::Success;
            }
        }
    }

private:
    VirtualFileSystem() = default;

    static NtStatus dispatchCreate(io::DeviceObject* dev, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidDeviceRequest;
        irp->ioStatus.status = NtStatus::Success;
        irp->ioStatus.information = 1; // FILE_OPENED
        return NtStatus::Success;
    }

    static NtStatus dispatchRead(io::DeviceObject* dev, io::Irp* irp) {
        if (!irp || !irp->userBuffer) return NtStatus::InvalidParameter;
        irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    }

    static NtStatus dispatchWrite(io::DeviceObject* dev, io::Irp* irp) {
        if (!irp || !irp->userBuffer) return NtStatus::InvalidParameter;
        irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    }

    static NtStatus dispatchClose(io::DeviceObject* dev, io::Irp* irp) {
        if (!irp) return NtStatus::InvalidParameter;
        irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    }

    static NtStatus dispatchFileSystemControl(io::DeviceObject* dev, io::Irp* irp) {
        (void)dev;
        if (!irp) return NtStatus::InvalidDeviceRequest;
        auto* stack = io::IoGetCurrentIrpStackLocation(irp);
        if (stack && stack->minorFunction == io::IRP_MN_MOUNT_VOLUME) {
            irp->ioStatus.status = NtStatus::Success;
            irp->ioStatus.information = 0;
            return NtStatus::Success;
        }
        irp->ioStatus.status = NtStatus::Success;
        return NtStatus::Success;
    }

    std::wstring normalizePath(std::wstring_view path) const {
        std::wstring res(path);
        // Strip DOS drive prefix like DosDevices/C: or ??/C: or C:
        if (res.starts_with(L"\\DosDevices\\C:\\")) {
            res = res.substr(14);
        } else if (res.starts_with(L"\\??\\C:\\")) {
            res = res.substr(7);
        } else if (res.starts_with(L"C:\\") || res.starts_with(L"c:\\")) {
            res = res.substr(3);
        } else if (res.starts_with(L"\\")) {
            res = res.substr(1);
        }
        return res;
    }

    std::shared_ptr<VfsEntry> findEntry(std::wstring_view path) const {
        if (path.empty() || path == L".") return rootEntry_;

        std::shared_ptr<VfsEntry> current = rootEntry_;
        size_t start = 0;
        while (start < path.size()) {
            size_t nextSlash = path.find(L'\\', start);
            std::wstring part = (nextSlash == std::wstring_view::npos) 
                ? std::wstring(path.substr(start)) 
                : std::wstring(path.substr(start, nextSlash - start));

            if (!part.empty()) {
                auto it = current->children.find(part);
                if (it == current->children.end()) return nullptr;
                current = it->second;
            }

            if (nextSlash == std::wstring_view::npos) break;
            start = nextSlash + 1;
        }
        return current;
    }

    std::shared_ptr<VfsEntry> createPathEntries(std::wstring_view path, bool isDir) {
        std::shared_ptr<VfsEntry> current = rootEntry_;
        size_t start = 0;
        while (start < path.size()) {
            size_t nextSlash = path.find(L'\\', start);
            std::wstring part = (nextSlash == std::wstring_view::npos) 
                ? std::wstring(path.substr(start)) 
                : std::wstring(path.substr(start, nextSlash - start));

            bool isLast = (nextSlash == std::wstring_view::npos);
            if (!part.empty()) {
                auto it = current->children.find(part);
                if (it == current->children.end()) {
                    auto newEntry = std::make_shared<VfsEntry>();
                    newEntry->name = part;
                    newEntry->isDirectory = isLast ? isDir : true;
                    newEntry->attributes = newEntry->isDirectory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
                    current->children[part] = newEntry;
                    current = newEntry;
                } else {
                    current = it->second;
                }
            }

            if (nextSlash == std::wstring_view::npos) break;
            start = nextSlash + 1;
        }
        return current;
    }

    void createDirectoryInternal(std::wstring_view path) {
        createPathEntries(path, true);
    }

    void createFileInternal(std::wstring_view path, std::span<const uint8_t> data) {
        auto entry = createPathEntries(path, false);
        if (entry) {
            entry->content.assign(data.begin(), data.end());
        }
    }

    bool initialized_{false};
    mutable std::mutex mutex_;
    std::unordered_map<wchar_t, DriveMapping> driveMappings_;
    std::unordered_map<std::string, wchar_t> ntfsSnapshots_;
    std::unique_ptr<io::DriverObject> fastFatDriver_;
    std::shared_ptr<io::DeviceObject> partitionDevice_;
    std::shared_ptr<VfsEntry> rootEntry_;
    std::unordered_map<FileObject*, std::shared_ptr<VfsEntry>> openFiles_;
    std::unordered_map<FileObject*, std::shared_ptr<npfs::NamedPipeInstance>> openPipeInstances_;
    std::unordered_map<FileObject*, std::shared_ptr<npfs::Mailslot>> openMailslots_;
    std::shared_ptr<storage::IBlockDevice> mountedDevice_;
    std::shared_ptr<fat32::Fat32FileSystem> fat32Fs_;
    std::shared_ptr<ntfs::NtfsFileSystem> ntfsFs_;
    std::unordered_map<FileObject*, std::pair<uint64_t, std::wstring>> openNtfsFiles_;
    std::shared_ptr<exfat::ExfatFileSystem> exfatFs_;
    std::unordered_map<FileObject*, std::wstring> openExfatFiles_;
    std::shared_ptr<cdfs::CdfsFileSystem> cdfsFs_;
    std::unordered_map<FileObject*, std::wstring> openCdfsFiles_;
    std::shared_ptr<refs::RefsFileSystem> refsFs_;
    std::unordered_map<FileObject*, uint64_t> openRefsFiles_;
    std::shared_ptr<zfs::ZfsFileSystem> zfsFs_;
    std::unordered_map<FileObject*, std::wstring> openZfsFiles_;
    std::shared_ptr<btrfs::BtrfsFileSystem> btrfsFs_;
    std::unordered_map<FileObject*, uint64_t> openBtrfsFiles_;
    std::shared_ptr<fs::ext::ExtFileSystem> extFs_;
    std::unordered_map<FileObject*, uint32_t> openExtFiles_;
    std::shared_ptr<xfs::XfsFileSystem> xfsFs_;
    std::unordered_map<FileObject*, uint64_t> openXfsFiles_;
    std::shared_ptr<f2fs::F2fsFileSystem> f2fsFs_;
    std::unordered_map<FileObject*, uint64_t> openF2fsFiles_;
    io::FastIoDispatch fastIoTable_{};
    std::unique_ptr<io::Vpb> vpb_;
};

} // namespace micant::fs

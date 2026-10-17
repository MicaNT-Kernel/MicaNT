/**
 * @file extfs.cpp
 * @brief Clean-Room Linux Ext2 / Ext3 / Ext4 Read-Only Filesystem Subsystem (Kernel Unit #91).
 */

#include "micant/km/drivers/fs/extfs.hpp"
#include "micant/km/drivers/fs/fs.hpp"
#include <iostream>

namespace micant::fs::ext {

// ============================================================================
// 1. Helper Utilities
// ============================================================================

namespace {

inline std::wstring StringToWString(std::string_view s) {
    std::wstring ws;
    ws.reserve(s.size());
    for (char c : s) {
        ws.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
    }
    return ws;
}

inline std::string WStringToString(std::wstring_view ws) {
    std::string s;
    s.reserve(ws.size());
    for (wchar_t wc : ws) {
        s.push_back(static_cast<char>(wc < 128 ? wc : '?'));
    }
    return s;
}

inline bool CaseInsensitiveEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] + 32) : a[i];
        char cb = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] + 32) : b[i];
        if (ca != cb) return false;
    }
    return true;
}

} // anonymous namespace

uint64_t ExtFileSystem::unixTimeToNtFileTime(uint32_t sec, uint32_t nsec) noexcept {
    // NT Epoch: Jan 1, 1601. Unix Epoch: Jan 1, 1970.
    // Difference in seconds: 11644473600ULL.
    // 1 second = 10,000,000 NT 100ns units.
    uint64_t ntUnits = (static_cast<uint64_t>(sec) + 11644473600ULL) * 10000000ULL;
    ntUnits += (nsec / 100);
    return ntUnits;
}

// ============================================================================
// 2. ExtFileSystem Implementation
// ============================================================================

bool ExtFileSystem::readBlock(uint64_t blockNum, void* buffer) {
    if (!device_ || !buffer) return false;
    uint64_t byteOffset = blockNum * blockSize_;
    return device_->readBytes(byteOffset, blockSize_, buffer);
}

NTSTATUS ExtFileSystem::mount(std::shared_ptr<IExtBlockDevice> device) {
    std::lock_guard<std::mutex> lock(fsMutex_);
    if (!device) {
        return STATUS_INVALID_PARAMETER;
    }

    device_ = device;

    // 1. Read Superblock at offset 1024
    if (!device_->readBytes(EXT_SUPERBLOCK_OFFSET, sizeof(ExtSuperblock), &sb_)) {
        return STATUS_DEVICE_DATA_ERROR;
    }

    // 2. Verify Superblock Magic
    if (sb_.s_magic != EXT_SUPER_MAGIC) {
        return STATUS_UNRECOGNIZED_VOLUME;
    }

    // 3. Block Size & Dimensions
    blockSize_ = 1024u << sb_.s_log_block_size;
    if (blockSize_ < 1024 || blockSize_ > 65536) {
        return STATUS_DISK_CORRUPT_ERROR;
    }

    inodeSize_ = (sb_.s_rev_level >= 1 && sb_.s_inode_size >= 128) ? sb_.s_inode_size : 128;
    inodesPerGroup_ = sb_.s_inodes_per_group;
    blocksPerGroup_ = sb_.s_blocks_per_group;

    if (inodesPerGroup_ == 0 || blocksPerGroup_ == 0) {
        return STATUS_DISK_CORRUPT_ERROR;
    }

    uint64_t totalBlocks = (static_cast<uint64_t>(sb_.s_blocks_count_hi) << 32) | sb_.s_blocks_count_lo;
    groupCount_ = static_cast<uint32_t>((totalBlocks + blocksPerGroup_ - 1) / blocksPerGroup_);

    is64Bit_ = (sb_.s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) != 0;
    hasExtents_ = (sb_.s_feature_incompat & EXT4_FEATURE_INCOMPAT_EXTENTS) != 0;
    descSize_ = is64Bit_ ? (sb_.s_desc_size >= 64 ? sb_.s_desc_size : 64) : 32;

    // 4. Read Group Descriptor Table (GDT)
    uint64_t gdtStartBlock = (blockSize_ == 1024) ? 2 : 1;
    size_t gdtSizeBytes = groupCount_ * descSize_;
    std::vector<uint8_t> gdtBuffer(gdtSizeBytes);

    uint64_t gdtByteOffset = gdtStartBlock * blockSize_;
    if (!device_->readBytes(gdtByteOffset, gdtSizeBytes, gdtBuffer.data())) {
        return STATUS_DEVICE_DATA_ERROR;
    }

    groups_.resize(groupCount_);
    for (uint32_t g = 0; g < groupCount_; ++g) {
        const uint8_t* descPtr = gdtBuffer.data() + (g * descSize_);
        if (descSize_ >= 64) {
            std::memcpy(&groups_[g], descPtr, sizeof(ExtBlockGroupDescriptor));
        } else {
            std::memset(&groups_[g], 0, sizeof(ExtBlockGroupDescriptor));
            std::memcpy(&groups_[g], descPtr, 32); // 32-byte legacy ext2/3 descriptor
        }
    }

    // 5. Volume Label
    char lbl[17] = {0};
    std::memcpy(lbl, sb_.s_volume_name, 16);
    volumeLabel_ = lbl;

    mounted_ = true;
    return STATUS_SUCCESS;
}

NTSTATUS ExtFileSystem::readInode(uint32_t inodeNum, ExtInode& outInode) {
    if (!mounted_) return STATUS_DEVICE_NOT_READY;
    if (inodeNum == 0 || inodeNum > sb_.s_inodes_count) return STATUS_NO_SUCH_FILE;

    uint32_t group = (inodeNum - 1) / inodesPerGroup_;
    uint32_t index = (inodeNum - 1) % inodesPerGroup_;

    if (group >= groups_.size()) return STATUS_NO_SUCH_FILE;

    const auto& desc = groups_[group];
    uint64_t tableBlock = desc.bg_inode_table_lo;
    if (is64Bit_) {
        tableBlock |= (static_cast<uint64_t>(desc.bg_inode_table_hi) << 32);
    }

    uint64_t inodeByteOffset = (tableBlock * blockSize_) + (index * inodeSize_);
    std::memset(&outInode, 0, sizeof(ExtInode));

    size_t bytesToRead = std::min<size_t>(inodeSize_, sizeof(ExtInode));
    if (!device_->readBytes(inodeByteOffset, bytesToRead, &outInode)) {
        return STATUS_DEVICE_DATA_ERROR;
    }

    return STATUS_SUCCESS;
}

uint64_t ExtFileSystem::traverseExtentTree(const uint8_t* extentData, uint32_t logicalBlock) {
    if (!extentData) return 0;

    const auto* hdr = reinterpret_cast<const Ext4ExtentHeader*>(extentData);
    if (hdr->eh_magic != EXT4_EXT_MAGIC) {
        return 0;
    }

    if (hdr->eh_depth == 0) {
        // Leaf node
        const auto* extents = reinterpret_cast<const Ext4Extent*>(extentData + sizeof(Ext4ExtentHeader));
        for (uint16_t i = 0; i < hdr->eh_entries; ++i) {
            uint32_t eeBlock = extents[i].ee_block;
            uint16_t eeLen = (extents[i].ee_len <= 32768) ? extents[i].ee_len : static_cast<uint16_t>(extents[i].ee_len - 32768);
            if (logicalBlock >= eeBlock && logicalBlock < (eeBlock + eeLen)) {
                uint64_t physStart = (static_cast<uint64_t>(extents[i].ee_start_hi) << 32) | extents[i].ee_start_lo;
                return physStart + (logicalBlock - eeBlock);
            }
        }
        return 0; // Sparse block hole
    }

    // Internal index node (depth > 0)
    const auto* indices = reinterpret_cast<const Ext4ExtentIdx*>(extentData + sizeof(Ext4ExtentHeader));
    int target = -1;
    for (uint16_t i = 0; i < hdr->eh_entries; ++i) {
        if (indices[i].ei_block <= logicalBlock) {
            target = i;
        } else {
            break;
        }
    }

    if (target < 0) return 0;

    uint64_t childBlock = (static_cast<uint64_t>(indices[target].ei_leaf_hi) << 32) | indices[target].ei_leaf_lo;
    std::vector<uint8_t> childBuf(blockSize_);
    if (!readBlock(childBlock, childBuf.data())) {
        return 0;
    }

    return traverseExtentTree(childBuf.data(), logicalBlock);
}

uint64_t ExtFileSystem::resolveLogicalBlock(const ExtInode& inode, uint32_t logicalBlock) {
    // 1. If inode uses ext4 extent tree
    if (inode.i_flags & EXT4_EXTENTS_FL) {
        const uint8_t* extentData = reinterpret_cast<const uint8_t*>(inode.i_block);
        return traverseExtentTree(extentData, logicalBlock);
    }

    // 2. Legacy Direct and Indirect Block Addressing (ext2 / ext3)
    uint32_t ptrsPerBlock = blockSize_ / sizeof(uint32_t);

    // Direct blocks (0 .. 11)
    if (logicalBlock < 12) {
        return inode.i_block[logicalBlock];
    }

    // Singly indirect block (12 .. 11 + ptrsPerBlock)
    uint32_t singlyLimit = 12 + ptrsPerBlock;
    if (logicalBlock < singlyLimit) {
        uint32_t indirectBlock = inode.i_block[12];
        if (indirectBlock == 0) return 0;
        std::vector<uint32_t> blockTable(ptrsPerBlock);
        if (!readBlock(indirectBlock, blockTable.data())) return 0;
        return blockTable[logicalBlock - 12];
    }

    // Doubly indirect block
    uint32_t doublyLimit = singlyLimit + (ptrsPerBlock * ptrsPerBlock);
    if (logicalBlock < doublyLimit) {
        uint32_t dIndirectBlock = inode.i_block[13];
        if (dIndirectBlock == 0) return 0;
        std::vector<uint32_t> dTable(ptrsPerBlock);
        if (!readBlock(dIndirectBlock, dTable.data())) return 0;

        uint32_t relBlock = logicalBlock - singlyLimit;
        uint32_t l1 = relBlock / ptrsPerBlock;
        uint32_t l2 = relBlock % ptrsPerBlock;

        uint32_t sBlock = dTable[l1];
        if (sBlock == 0) return 0;
        std::vector<uint32_t> sTable(ptrsPerBlock);
        if (!readBlock(sBlock, sTable.data())) return 0;
        return sTable[l2];
    }

    return 0;
}

NTSTATUS ExtFileSystem::readFileBytes(const ExtInode& inode, uint64_t offset, size_t length, void* buffer, size_t* bytesRead) {
    if (!mounted_) return STATUS_DEVICE_NOT_READY;
    if (!buffer) return STATUS_INVALID_PARAMETER;

    uint64_t fileSize = (static_cast<uint64_t>(inode.i_size_high) << 32) | inode.i_size_lo;
    if (offset >= fileSize) {
        if (bytesRead) *bytesRead = 0;
        return STATUS_END_OF_FILE;
    }

    size_t toRead = static_cast<size_t>(std::min<uint64_t>(length, fileSize - offset));
    uint8_t* dst = reinterpret_cast<uint8_t*>(buffer);
    size_t copied = 0;

    std::vector<uint8_t> blockBuf(blockSize_);

    while (copied < toRead) {
        uint64_t curOffset = offset + copied;
        uint32_t logicalBlock = static_cast<uint32_t>(curOffset / blockSize_);
        uint32_t blockOffset = static_cast<uint32_t>(curOffset % blockSize_);
        size_t chunk = std::min<size_t>(toRead - copied, blockSize_ - blockOffset);

        uint64_t physBlock = resolveLogicalBlock(inode, logicalBlock);
        if (physBlock == 0) {
            // Sparse hole
            std::memset(dst + copied, 0, chunk);
        } else {
            if (!readBlock(physBlock, blockBuf.data())) {
                return STATUS_DEVICE_DATA_ERROR;
            }
            std::memcpy(dst + copied, blockBuf.data() + blockOffset, chunk);
        }

        copied += chunk;
    }

    if (bytesRead) *bytesRead = copied;
    return STATUS_SUCCESS;
}

NTSTATUS ExtFileSystem::readSymlink(const ExtInode& inode, std::string& target) {
    if ((inode.i_mode & EXT_S_IFMT) != EXT_S_IFLNK) {
        return STATUS_INVALID_PARAMETER;
    }

    uint64_t sz = inode.i_size_lo;
    if (sz < 60) {
        // Fast symlink directly inside i_block
        target.assign(reinterpret_cast<const char*>(inode.i_block), static_cast<size_t>(sz));
        return STATUS_SUCCESS;
    }

    // Extent or block backed symlink
    target.resize(static_cast<size_t>(sz));
    size_t readCount = 0;
    return readFileBytes(inode, 0, static_cast<size_t>(sz), target.data(), &readCount);
}

NTSTATUS ExtFileSystem::listDirectory(uint32_t dirInodeNum, std::vector<ExtFsDirEntry>& entries) {
    ExtInode dirInode{};
    NTSTATUS status = readInode(dirInodeNum, dirInode);
    if (!NT_SUCCESS(status)) return status;

    if ((dirInode.i_mode & EXT_S_IFMT) != EXT_S_IFDIR) {
        return STATUS_NOT_A_DIRECTORY;
    }

    uint64_t dirSize = dirInode.i_size_lo;
    std::vector<uint8_t> dirData(static_cast<size_t>(dirSize));
    size_t bytesRead = 0;
    status = readFileBytes(dirInode, 0, static_cast<size_t>(dirSize), dirData.data(), &bytesRead);
    if (!NT_SUCCESS(status) && status != STATUS_END_OF_FILE) {
        return status;
    }

    entries.clear();
    size_t offset = 0;
    while (offset + sizeof(Ext4DirEntry2) <= bytesRead) {
        const auto* de = reinterpret_cast<const Ext4DirEntry2*>(dirData.data() + offset);
        if (de->rec_len == 0 || offset + de->rec_len > bytesRead) {
            break;
        }

        if (de->inode != 0 && de->name_len > 0 && (offset + 8 + de->name_len <= bytesRead)) {
            std::string name(de->name, de->name_len);
            ExtFsDirEntry ent{};
            ent.inode = de->inode;
            ent.fileType = de->file_type;
            ent.name = name;
            ent.wideName = StringToWString(name);

            ExtInode targetInode{};
            if (NT_SUCCESS(readInode(de->inode, targetInode))) {
                ent.fileSize = (static_cast<uint64_t>(targetInode.i_size_high) << 32) | targetInode.i_size_lo;
                ent.mode = targetInode.i_mode;
                ent.creationTime = unixTimeToNtFileTime(targetInode.i_ctime, targetInode.i_ctime_extra);
                ent.lastWriteTime = unixTimeToNtFileTime(targetInode.i_mtime, targetInode.i_mtime_extra);
                ent.isDirectory = (targetInode.i_mode & EXT_S_IFMT) == EXT_S_IFDIR;
                ent.isSymlink = (targetInode.i_mode & EXT_S_IFMT) == EXT_S_IFLNK;
            }

            entries.push_back(std::move(ent));
        }

        offset += de->rec_len;
    }

    return STATUS_SUCCESS;
}

NTSTATUS ExtFileSystem::resolvePath(std::string_view path, uint32_t& outInodeNum, ExtInode* outInode) {
    if (!mounted_) return STATUS_DEVICE_NOT_READY;

    // Normalize path separators
    uint32_t currentInode = EXT_ROOT_INO;

    size_t start = 0;
    while (start < path.size()) {
        while (start < path.size() && (path[start] == '/' || path[start] == '\\')) {
            start++;
        }
        if (start >= path.size()) break;

        size_t end = start;
        while (end < path.size() && path[end] != '/' && path[end] != '\\') {
            end++;
        }

        std::string_view segment = path.substr(start, end - start);
        start = end;

        if (segment.empty() || segment == ".") continue;

        std::vector<ExtFsDirEntry> entries;
        NTSTATUS st = listDirectory(currentInode, entries);
        if (!NT_SUCCESS(st)) return st;

        bool found = false;
        // 1. Exact match
        for (const auto& ent : entries) {
            if (ent.name == segment) {
                currentInode = ent.inode;
                found = true;
                break;
            }
        }

        // 2. Case-insensitive fallback for Win32 compatibility
        if (!found) {
            for (const auto& ent : entries) {
                if (CaseInsensitiveEqual(ent.name, segment)) {
                    currentInode = ent.inode;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
    }

    outInodeNum = currentInode;
    if (outInode) {
        return readInode(currentInode, *outInode);
    }
    return STATUS_SUCCESS;
}

NTSTATUS ExtFileSystem::resolveWin32Path(std::wstring_view path, uint32_t& outInodeNum, ExtInode* outInode) {
    std::string sPath = WStringToString(path);
    // Strip drive prefix like "E:\" or "L:\"
    if (sPath.size() >= 2 && sPath[1] == ':') {
        sPath = sPath.substr(2);
    }
    return resolvePath(sPath, outInodeNum, outInode);
}

// ============================================================================
// 3. NT Installable File System (IFS) Driver Wrapper
// ============================================================================

void ExtFsDriver::initialize() {
    std::lock_guard<std::mutex> lock(driverMutex_);
    if (initialized_) return;

    driverObject_ = std::make_unique<io::DriverObject>();
    driverObject_->driverName = L"\\FileSystem\\ExtFs";

    driverObject_->setDispatch(io::IRP_MJ_CREATE, &ExtFsDriver::dispatchCreate);
    driverObject_->setDispatch(io::IRP_MJ_READ, &ExtFsDriver::dispatchRead);
    driverObject_->setDispatch(io::IRP_MJ_WRITE, &ExtFsDriver::dispatchWrite);
    driverObject_->setDispatch(io::IRP_MJ_CLOSE, &ExtFsDriver::dispatchClose);
    driverObject_->setDispatch(io::IRP_MJ_DIRECTORY_CONTROL, &ExtFsDriver::dispatchDirectoryControl);
    driverObject_->setDispatch(io::IRP_MJ_QUERY_INFORMATION, &ExtFsDriver::dispatchQueryInformation);
    driverObject_->setDispatch(io::IRP_MJ_FILE_SYSTEM_CONTROL, &ExtFsDriver::dispatchFileSystemControl);

    initialized_ = true;
}

NTSTATUS ExtFsDriver::registerVolume(std::shared_ptr<ExtFileSystem> extFs, std::wstring_view volumeName) {
    initialize();
    std::lock_guard<std::mutex> lock(driverMutex_);
    if (!extFs || !extFs->isMounted()) return STATUS_INVALID_PARAMETER;

    mountedDrives_[std::wstring(volumeName)] = extFs;
    return STATUS_SUCCESS;
}

NTSTATUS ExtFsDriver::mountDrive(std::wstring_view driveLetter, std::shared_ptr<ExtFileSystem> extFs) {
    initialize();
    std::lock_guard<std::mutex> lock(driverMutex_);
    if (!extFs || !extFs->isMounted()) return STATUS_INVALID_PARAMETER;

    std::wstring dl(driveLetter);
    if (!dl.empty() && dl.back() != L':') {
        dl.push_back(L':');
    }
    mountedDrives_[dl] = extFs;

    // Integrate with VirtualFileSystem and IoManager
    auto volDev = io::IoManager::get().createDevice(
        driverObject_.get(),
        std::wstring(L"\\Device\\ExtFs_") + dl[0],
        io::DeviceType::FileSystem
    );
    (void)volDev;

    return STATUS_SUCCESS;
}

std::shared_ptr<ExtFileSystem> ExtFsDriver::getFileSystem(std::wstring_view driveLetter) const {
    std::lock_guard<std::mutex> lock(driverMutex_);
    std::wstring dl(driveLetter);
    if (!dl.empty() && dl.back() != L':') {
        dl.push_back(L':');
    }
    auto it = mountedDrives_.find(dl);
    if (it != mountedDrives_.end()) {
        return it->second;
    }
    return nullptr;
}

NtStatus ExtFsDriver::dispatchCreate(io::DeviceObject* device, io::Irp* irp) {
    if (!irp) return NtStatus::InvalidParameter;

    // Strict Read-Only Guardrail: Reject any create disposition requesting write access
    uint32_t desiredAccess = irp->ioStatus.information;
    if ((desiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_GENERIC_WRITE)) != 0) {
        irp->ioStatus.status = NtStatus::MediaWriteProtected;
        return NtStatus::MediaWriteProtected;
    }

    irp->ioStatus.status = NtStatus::Success;
    irp->ioStatus.information = 1; // FILE_OPENED
    return NtStatus::Success;
}

NtStatus ExtFsDriver::dispatchRead(io::DeviceObject* device, io::Irp* irp) {
    if (!irp) return NtStatus::InvalidParameter;
    irp->ioStatus.status = NtStatus::Success;
    return NtStatus::Success;
}

NtStatus ExtFsDriver::dispatchWrite(io::DeviceObject* /*device*/, io::Irp* irp) {
    // Sovereign Read-Only Guardrail: All writes rejected unconditionally
    if (irp) {
        irp->ioStatus.status = NtStatus::MediaWriteProtected;
        irp->ioStatus.information = 0;
    }
    return NtStatus::MediaWriteProtected;
}

NtStatus ExtFsDriver::dispatchClose(io::DeviceObject* /*device*/, io::Irp* irp) {
    if (irp) irp->ioStatus.status = NtStatus::Success;
    return NtStatus::Success;
}

NtStatus ExtFsDriver::dispatchDirectoryControl(io::DeviceObject* /*device*/, io::Irp* irp) {
    if (irp) irp->ioStatus.status = NtStatus::Success;
    return NtStatus::Success;
}

NtStatus ExtFsDriver::dispatchQueryInformation(io::DeviceObject* /*device*/, io::Irp* irp) {
    if (irp) irp->ioStatus.status = NtStatus::Success;
    return NtStatus::Success;
}

NtStatus ExtFsDriver::dispatchFileSystemControl(io::DeviceObject* /*device*/, io::Irp* irp) {
    if (irp) irp->ioStatus.status = NtStatus::Success;
    return NtStatus::Success;
}

} // namespace micant::fs::ext

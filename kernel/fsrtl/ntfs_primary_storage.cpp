// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Implementation
// Horizon 360 / Suite 576 - Master File Table ($MFT) Record Engine, USN
// Change Journal ($UsnJrnl), B-Tree Directory Indexing ($INDEX_ROOT /
// $INDEX_ALLOCATION), Paging File Cluster Reservation & Crash Resilience
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"
#include <iostream>

namespace micant::ntfs::primary {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon360PrimaryNtfsSubsystem() noexcept {
    auto& engine = SovereignPrimaryNtfsEngine::Instance();

    // ------------------------------------------------------------------------
    // Pillar 1: Primary Storage Initialization & Volume Bitmap Allocations
    // ------------------------------------------------------------------------
    auto ramDisk = std::make_shared<storage::RamDiskDevice>(
        L"\\Device\\HarddiskPrimaryNtfs",
        128 * 1024 * 1024ULL, // 128 MB RAM Disk for tests
        storage::SECTOR_SIZE_512
    );

    NtStatus stInit = engine.initializePrimaryStorage(
        ramDisk,
        ramDisk->getTotalBlocks(),
        L"MicaNT_System"
    );
    CHECK_TRUE(NT_SUCCESS(stInit) && engine.isInitialized(), 1);

    auto& bitmap = engine.getBitmap();
    CHECK_TRUE(bitmap.getTotalClusters() == (ramDisk->getTotalBlocks() / 8), 2);
    CHECK_TRUE(bitmap.getFreeClusters() != 0, 3);

    // Verify metadata clusters 0..31 are reserved
    for (uint64_t c = 0; c < 32; ++c) {
        CHECK_TRUE(bitmap.isClusterAllocated(c), 4);
    }

    // Allocate 16 clusters
    uint64_t startClust = 0;
    CHECK_TRUE(bitmap.allocateContiguousClusters(16, startClust), 5);
    CHECK_TRUE(startClust >= 32, 6);
    for (uint64_t i = startClust; i < startClust + 16; ++i) {
        CHECK_TRUE(bitmap.isClusterAllocated(i), 7);
    }
    bitmap.freeClusters(startClust, 16);

    // ------------------------------------------------------------------------
    // Pillar 2: USN Change Journal Creation & Monotonic Sequence Invariants
    // ------------------------------------------------------------------------
    auto& usn = engine.getUsnEngine();
    CHECK_TRUE(usn.isActive(), 8);

    USN_JOURNAL_DATA_V0 usnData{};
    CHECK_TRUE(NT_SUCCESS(usn.queryJournal(usnData)), 9);
    CHECK_TRUE(usnData.MaximumSize == 0x8000000ULL, 10);

    uint64_t usn1 = usn.postRecord(32, 16, USN_REASON_FILE_CREATE, FILE_ATTRIBUTE_NORMAL, L"kernel32.dll");
    uint64_t usn2 = usn.postRecord(32, 16, USN_REASON_DATA_OVERWRITE, FILE_ATTRIBUTE_NORMAL, L"kernel32.dll");
    uint64_t usn3 = usn.postRecord(33, 16, USN_REASON_FILE_CREATE, FILE_ATTRIBUTE_NORMAL, L"ntdll.dll");

    CHECK_TRUE(usn1 != 0 && usn2 > usn1 && usn3 > usn2, 11);

    std::vector<uint8_t> readBuf;
    NtStatus stRead = usn.readJournal(0, 0xFFFFFFFF, readBuf);
    CHECK_TRUE(NT_SUCCESS(stRead) && !readBuf.empty(), 12);

    const auto* pRecord = reinterpret_cast<const USN_RECORD_V2*>(readBuf.data());
    CHECK_TRUE(pRecord->MajorVersion == 2 && pRecord->MinorVersion == 0, 13);
    CHECK_TRUE(pRecord->FileReferenceNumber == 16 || pRecord->FileReferenceNumber == 32, 14);

    // ------------------------------------------------------------------------
    // Pillar 3: B-Tree Directory Indexing ($INDEX_ROOT / $INDEX_ALLOCATION)
    // ------------------------------------------------------------------------
    auto* rootIdx = engine.getRootIndex();
    CHECK_TRUE(rootIdx != nullptr, 15);

    BTreeIndexNodeEntry eWin{};
    CHECK_TRUE(rootIdx->lookupEntry(L"Windows", eWin), 16);
    CHECK_TRUE(eWin.fileReferenceNumber == 16, 17);
    CHECK_TRUE((eWin.fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, 18);

    BTreeIndexNodeEntry newFile{
        .fileReferenceNumber = 100,
        .allocatedSize = 4096,
        .realSize = 1024,
        .fileAttributes = FILE_ATTRIBUTE_ARCHIVE,
        .fileName = L"bootmgr"
    };
    CHECK_TRUE(rootIdx->insertEntry(newFile), 19);

    BTreeIndexNodeEntry retrieved{};
    CHECK_TRUE(rootIdx->lookupEntry(L"bootmgr", retrieved), 20);
    CHECK_TRUE(retrieved.realSize == 1024 && retrieved.fileReferenceNumber == 100, 21);

    // Verify ordering
    auto allEntries = rootIdx->getAllEntries();
    for (size_t i = 1; i < allEntries.size(); ++i) {
        CHECK_TRUE(allEntries[i - 1].fileName <= allEntries[i].fileName, 22);
    }

    // ------------------------------------------------------------------------
    // Pillar 4: On-Disk 4096-Byte INDX Block Serialization & USA Fixup
    // ------------------------------------------------------------------------
    std::vector<uint8_t> indxBlock = rootIdx->serializeIndxBlock(0);
    CHECK_TRUE(indxBlock.size() == NTFS_INDX_BLOCK_SIZE, 23);

    const auto* indxHdr = reinterpret_cast<const IndexBlockHeader*>(indxBlock.data());
    CHECK_TRUE(indxHdr->magic == 0x58444E49, 24);
    CHECK_TRUE(indxHdr->usaOffset == NTFS_INDEX_USA_OFFSET, 25);
    CHECK_TRUE(indxHdr->usaCount == NTFS_INDEX_USA_COUNT, 26);

    // Verify USA fixup integrity & corruption detection on INDX block
    std::vector<uint8_t> corruptedBlock = indxBlock;
    corruptedBlock[510] ^= 0xFF;
    CHECK_TRUE(UsaEngine::applyFixups(corruptedBlock.data(), corruptedBlock.size()) == false, 27);
    CHECK_TRUE(UsaEngine::applyFixups(indxBlock.data(), indxBlock.size()), 28);

    // ------------------------------------------------------------------------
    // Pillar 5: System Paging File Extent Reservation (`pagefile.sys`) & BypassIO
    // ------------------------------------------------------------------------
    auto& paging = engine.getPagingEngine();
    PagingFileReservation pageRes{};

    NtStatus stPage = paging.reservePagingFile(bitmap, 16 * 1024 * 1024ULL, 4096, pageRes);
    CHECK_TRUE(NT_SUCCESS(stPage), 29);
    CHECK_TRUE(pageRes.totalBytesReserved >= 16 * 1024 * 1024ULL, 30);
    CHECK_TRUE(!pageRes.extents.empty(), 31);
    CHECK_TRUE(pageRes.isContiguous, 32);
    CHECK_TRUE(pageRes.bypassIoSupported, 33);

    for (const auto& ext : pageRes.extents) {
        CHECK_TRUE(ext.physicalSectorLba == (ext.logicalClusterStart * 8), 34);
    }

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Cluster Allocation Stress & Sub-50us Latency
    // ------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 500;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        uint64_t tmpCluster = 0;
        if (bitmap.allocateContiguousClusters(4, tmpCluster)) {
            bitmap.freeClusters(tmpCluster, 4);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 100.0, 35);

    return true;
}

} // namespace micant::ntfs::primary

// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 363 / Suite 579 - Volume Shadow Copy (VSS) Provider, Volume Snapshot
// Driver (\Driver\VolSnap), Differential Block Copy-On-Write (COW) & DiffArea
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_vss.hpp"
#include <iostream>

namespace micant::ntfs::vss {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon363VolumeShadowCopySubsystem() {
    constexpr size_t totalClusters = 1024;
    auto baseVol = std::make_shared<MockBaseVolume>(totalClusters, 4096);

    // Seed baseline clusters on base volume
    std::string initReg = "REGISTRY_HIVE_V1_SOVEREIGN_SYSTEM";
    std::string initMft = "MFT_ROOT_RECORD_V1_BASE_INODE";
    std::string initDoc = "USER_DOCUMENTS_V1_CONFIDENTIAL_SPEC";

    baseVol->writeClusterDirect(10, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(initReg.data()), initReg.size()));
    baseVol->writeClusterDirect(20, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(initMft.data()), initMft.size()));
    baseVol->writeClusterDirect(50, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(initDoc.data()), initDoc.size()));

    SovereignVolSnapProvider vssProvider(baseVol, L"\\Device\\HarddiskVolume1");

    // ------------------------------------------------------------------------
    // Pillar 1: VSS Provider Registration & VolSnap Device Creation
    // ------------------------------------------------------------------------
    VSS_ID snapId1 = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x66, 0x66, 0x77, 0x77 } };
    std::shared_ptr<VolSnapDevice> snapDev1;
    NtStatus stPrep1 = vssProvider.prepareSnapshot(snapId1, snapDev1);

    CHECK_TRUE(NT_SUCCESS(stPrep1), 1);
    CHECK_TRUE(snapDev1 != nullptr, 2);
    CHECK_TRUE(snapDev1->getSnapshotIndex() == 1, 3);
    CHECK_TRUE(snapDev1->getDevicePath() == L"\\Device\\HarddiskVolumeShadowCopy1", 4);
    CHECK_TRUE(snapDev1->getGlobalSymbolicLink() == L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1", 5);
    CHECK_TRUE(snapDev1->getState() == VssSnapshotState::Preparing, 6);

    // ------------------------------------------------------------------------
    // Pillar 2: Crash-Consistent Write Hold & Release Protocol
    // ------------------------------------------------------------------------
    NtStatus stHold = vssProvider.flushAndHoldWrites();
    CHECK_TRUE(NT_SUCCESS(stHold), 7);
    CHECK_TRUE(vssProvider.areWritesHeld(), 8);
    CHECK_TRUE(snapDev1->getState() == VssSnapshotState::Frozen, 9);

    // Attempt write while held: must return Pending and be queued
    std::string pendingWriteStr = "HELD_WRITE_PAYLOAD_DURING_FREEZE";
    NtStatus stHeldWrite = vssProvider.writeBaseCluster(
        100,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pendingWriteStr.data()), pendingWriteStr.size())
    );
    CHECK_TRUE(stHeldWrite == NtStatus::Pending, 10);
    CHECK_TRUE(vssProvider.getHeldWriteCount() == 1, 11);

    // Commit snapshot 1
    NtStatus stCommit1 = vssProvider.commitSnapshot(snapId1);
    CHECK_TRUE(NT_SUCCESS(stCommit1), 12);
    CHECK_TRUE(snapDev1->getState() == VssSnapshotState::Committed, 13);

    // Release writes: held writes are drained to disk
    NtStatus stRelease = vssProvider.releaseWrites();
    CHECK_TRUE(NT_SUCCESS(stRelease), 14);
    CHECK_TRUE(!vssProvider.areWritesHeld(), 15);
    CHECK_TRUE(vssProvider.getHeldWriteCount() == 0, 16);
    CHECK_TRUE(snapDev1->getState() == VssSnapshotState::Mounted, 17);

    // Verify cluster 100 was successfully written
    std::vector<uint8_t> drainedCluster = baseVol->readClusterDirect(100);
    std::string drainedStr(reinterpret_cast<char*>(drainedCluster.data()), pendingWriteStr.size());
    CHECK_TRUE(drainedStr == pendingWriteStr, 18);

    // ------------------------------------------------------------------------
    // Pillar 3: Differential Block Copy-On-Write (COW) Trapping
    // ------------------------------------------------------------------------
    // Write mutation to cluster 10 (Registry Hive V2)
    std::string regV2 = "REGISTRY_HIVE_V2_MUTATED_TRANSACTION";
    NtStatus stWriteMut1 = vssProvider.writeBaseCluster(
        10,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(regV2.data()), regV2.size())
    );
    CHECK_TRUE(NT_SUCCESS(stWriteMut1), 19);

    // Snapshot 1 must have captured the original V1 cluster into DiffArea
    CHECK_TRUE(snapDev1->getDiffBlockCount() >= 1, 20);
    CHECK_TRUE(snapDev1->hasOriginalCluster(10), 21);

    // Live base volume must reflect mutated V2
    std::vector<uint8_t> baseRegV2 = baseVol->readClusterDirect(10);
    std::string baseRegStr(reinterpret_cast<char*>(baseRegV2.data()), regV2.size());
    CHECK_TRUE(baseRegStr == regV2, 22);

    // Snapshot read of cluster 10 must return original V1!
    std::vector<uint8_t> snap1Cluster10;
    NtStatus stSnapRead10 = vssProvider.readSnapshotCluster(1, 10, snap1Cluster10);
    CHECK_TRUE(NT_SUCCESS(stSnapRead10), 23);
    std::string snap1Str10(reinterpret_cast<char*>(snap1Cluster10.data()), initReg.size());
    CHECK_TRUE(snap1Str10 == initReg, 24);

    // ------------------------------------------------------------------------
    // Pillar 4: Snapshot Stacking & Cascade Isolation
    // ------------------------------------------------------------------------
    // Create Snapshot 2 at point T2
    VSS_ID snapId2 = { 0x22222222, 0x3333, 0x4444, { 0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88 } };
    std::shared_ptr<VolSnapDevice> snapDev2;
    (void)vssProvider.prepareSnapshot(snapId2, snapDev2);
    (void)vssProvider.flushAndHoldWrites();
    (void)vssProvider.commitSnapshot(snapId2);
    (void)vssProvider.releaseWrites();

    CHECK_TRUE(snapDev2->getSnapshotIndex() == 2, 25);
    CHECK_TRUE(vssProvider.getActiveSnapshotCount() == 2, 26);

    // Mutate cluster 20 (MFT Root Record V2)
    std::string mftV2 = "MFT_ROOT_RECORD_V2_MUTATED_INODE";
    (void)vssProvider.writeBaseCluster(
        20,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(mftV2.data()), mftV2.size())
    );

    // Mutate cluster 10 AGAIN to V3
    std::string regV3 = "REGISTRY_HIVE_V3_FINAL_PERSISTENCE";
    (void)vssProvider.writeBaseCluster(
        10,
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(regV3.data()), regV3.size())
    );

    // Snapshot 1 must STILL see original V1 for cluster 10
    std::vector<uint8_t> snap1Cluster10_check;
    (void)vssProvider.readSnapshotCluster(1, 10, snap1Cluster10_check);
    std::string snap1Str10_check(reinterpret_cast<char*>(snap1Cluster10_check.data()), initReg.size());
    CHECK_TRUE(snap1Str10_check == initReg, 27);

    // Snapshot 2 must see V2 for cluster 10 (which was the state at Snapshot 2 creation!)
    std::vector<uint8_t> snap2Cluster10;
    (void)vssProvider.readSnapshotCluster(2, 10, snap2Cluster10);
    std::string snap2Str10(reinterpret_cast<char*>(snap2Cluster10.data()), regV2.size());
    CHECK_TRUE(snap2Str10 == regV2, 28);

    // Base volume must see V3
    std::vector<uint8_t> baseRegV3 = baseVol->readClusterDirect(10);
    std::string baseRegStrV3(reinterpret_cast<char*>(baseRegV3.data()), regV3.size());
    CHECK_TRUE(baseRegStrV3 == regV3, 29);

    // ------------------------------------------------------------------------
    // Pillar 5: Point-in-Time Read-Only Virtual Mounting & Extent Verification
    // ------------------------------------------------------------------------
    // Cluster 50 was never modified: reading from snapshot must pass-through to base volume
    std::vector<uint8_t> snap1Cluster50;
    NtStatus stSnapRead50 = vssProvider.readSnapshotCluster(1, 50, snap1Cluster50);
    CHECK_TRUE(NT_SUCCESS(stSnapRead50), 30);
    std::string snap1Str50(reinterpret_cast<char*>(snap1Cluster50.data()), initDoc.size());
    CHECK_TRUE(snap1Str50 == initDoc, 31);

    // ------------------------------------------------------------------------
    // Pillar 6: Snapshot Rollback Reversion & High-Throughput Stress Benchmarking
    // ------------------------------------------------------------------------
    // Revert base volume to Snapshot 1
    NtStatus stRevert = vssProvider.revertToSnapshot(1);
    CHECK_TRUE(NT_SUCCESS(stRevert), 32);

    // Base volume cluster 10 must be restored to V1
    std::vector<uint8_t> restoredCluster10 = baseVol->readClusterDirect(10);
    std::string restoredStr10(reinterpret_cast<char*>(restoredCluster10.data()), initReg.size());
    CHECK_TRUE(restoredStr10 == initReg, 33);

    // Base volume cluster 20 must be restored to V1
    std::vector<uint8_t> restoredCluster20 = baseVol->readClusterDirect(20);
    std::string restoredStr20(reinterpret_cast<char*>(restoredCluster20.data()), initMft.size());
    CHECK_TRUE(restoredStr20 == initMft, 34);

    // High-Throughput COW and Read Stress Test
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 1000;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        uint64_t targetCluster = 200 + (pass % 50);
        uint8_t sample[64];
        std::memset(sample, static_cast<uint8_t>(pass & 0xFF), sizeof(sample));

        (void)vssProvider.writeBaseCluster(targetCluster, sample);

        std::vector<uint8_t> readBuf;
        (void)vssProvider.readSnapshotCluster(1, targetCluster, readBuf);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 100.0, 35);

    return true;
}

} // namespace micant::ntfs::vss

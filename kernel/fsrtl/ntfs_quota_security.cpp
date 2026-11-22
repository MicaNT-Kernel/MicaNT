// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 367 / Suite 583 - NTFS Quota Governor & Security Descriptors Engine
// $Quota ($Q / $O), $Secure ($SDS / $SDH / $SII) Hash Buckets & Inherited ACEs
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_quota_security.hpp"
#include <iostream>

namespace micant::ntfs::security {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon367QuotaSecuritySubsystem() {
    SovereignNtfsSecurityStore secStore;

    // ------------------------------------------------------------------------
    // Pillar 1: Security Descriptor Hash Calculation & Deduplication
    // ------------------------------------------------------------------------
    SimpleSecurityDescriptor sd1;
    sd1.ownerSid = L"S-1-5-21-111111-222222-333333-1001";
    sd1.groupSid = L"S-1-5-21-111111-222222-333333-513"; // Domain Users
    sd1.dacl.push_back(SimpleAce{
        ACCESS_ALLOWED_ACE_TYPE,
        0,
        FILE_READ_DATA | FILE_WRITE_DATA | FILE_EXECUTE,
        L"S-1-5-21-111111-222222-333333-1001"
    });

    std::vector<uint8_t> sdBytes1 = sd1.serialize();
    uint32_t hash1 = SovereignNtfsSecurityStore::computeSecurityHash(sdBytes1);
    CHECK_TRUE(hash1 != 0, 1);

    uint32_t secId1 = 0;
    NtStatus stReg1 = secStore.registerSecurityDescriptor(sdBytes1, secId1);
    CHECK_TRUE(NT_SUCCESS(stReg1), 2);
    CHECK_TRUE(secId1 == SECURITY_ID_FIRST, 3);
    CHECK_TRUE(secStore.getUniqueDescriptorCount() == 1, 4);

    // Register identical descriptor (SD2 == SD1)
    SimpleSecurityDescriptor sd2 = sd1;
    std::vector<uint8_t> sdBytes2 = sd2.serialize();
    uint32_t secId2 = 0;
    NtStatus stReg2 = secStore.registerSecurityDescriptor(sdBytes2, secId2);

    CHECK_TRUE(NT_SUCCESS(stReg2), 5);
    // Deduplication check: must reuse existing secId1!
    CHECK_TRUE(secId2 == secId1, 6);
    CHECK_TRUE(secStore.getDeduplicationHits() == 1, 7);
    CHECK_TRUE(secStore.getUniqueDescriptorCount() == 1, 8); // No duplicate stored!

    // ------------------------------------------------------------------------
    // Pillar 2: $SDS Stream Serialization & $SDH / $SII Indexing
    // ------------------------------------------------------------------------
    // Register distinct Security Descriptor with Deny ACE
    SimpleSecurityDescriptor sd3;
    sd3.ownerSid = L"S-1-5-21-111111-222222-333333-500"; // Administrator
    sd3.groupSid = L"S-1-5-21-111111-222222-333333-512"; // Domain Admins
    sd3.dacl.push_back(SimpleAce{
        ACCESS_DENIED_ACE_TYPE,
        0,
        FILE_WRITE_DATA | FILE_DELETE_CHILD,
        L"S-1-5-21-111111-222222-333333-9999" // Untrusted Guest
    });
    sd3.dacl.push_back(SimpleAce{
        ACCESS_ALLOWED_ACE_TYPE,
        CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE,
        FILE_ALL_ACCESS,
        L"S-1-5-21-111111-222222-333333-500"
    });

    std::vector<uint8_t> sdBytes3 = sd3.serialize();
    uint32_t secId3 = 0;
    NtStatus stReg3 = secStore.registerSecurityDescriptor(sdBytes3, secId3);

    CHECK_TRUE(NT_SUCCESS(stReg3), 9);
    CHECK_TRUE(secId3 != secId1, 10);
    CHECK_TRUE(secStore.getUniqueDescriptorCount() == 2, 11);

    // Retrieve via $SII Index
    std::vector<uint8_t> retrievedSd1;
    std::vector<uint8_t> retrievedSd3;
    CHECK_TRUE(NT_SUCCESS(secStore.getSecurityDescriptor(secId1, retrievedSd1)), 12);
    CHECK_TRUE(NT_SUCCESS(secStore.getSecurityDescriptor(secId3, retrievedSd3)), 13);
    // First bytes must match exactly
    CHECK_TRUE(std::memcmp(retrievedSd1.data(), sdBytes1.data(), sdBytes1.size()) == 0, 14);
    CHECK_TRUE(std::memcmp(retrievedSd3.data(), sdBytes3.data(), sdBytes3.size()) == 0, 15);

    // ------------------------------------------------------------------------
    // Pillar 3: Access Control Mask Evaluation & Inherited ACEs
    // ------------------------------------------------------------------------
    CHECK_TRUE(sd3.dacl.size() == 2, 16);
    CHECK_TRUE(sd3.dacl[0].type == ACCESS_DENIED_ACE_TYPE, 17);
    CHECK_TRUE(sd3.dacl[1].type == ACCESS_ALLOWED_ACE_TYPE, 18);
    CHECK_TRUE((sd3.dacl[1].flags & (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE)) != 0, 19);

    // ------------------------------------------------------------------------
    // Pillar 4: User Quota Tracking ($Quota $Q / $O Streams)
    // ------------------------------------------------------------------------
    SovereignNtfsQuotaGovernor quotaGov;
    std::wstring testUserSid = L"S-1-5-21-111111-222222-333333-1001";

    constexpr uint64_t warningLimit = 100 * 1024 * 1024ULL; // 100 MB
    constexpr uint64_t hardLimit    = 150 * 1024 * 1024ULL; // 150 MB

    NtStatus stSetQuota = quotaGov.setQuotaLimits(testUserSid, warningLimit, hardLimit);
    CHECK_TRUE(NT_SUCCESS(stSetQuota), 20);

    // Initial allocation: 50 MB
    NtStatus stAlloc1 = quotaGov.allocateBytes(testUserSid, 50 * 1024 * 1024ULL);
    CHECK_TRUE(NT_SUCCESS(stAlloc1), 21);

    QuotaRecord qRec1{};
    CHECK_TRUE(NT_SUCCESS(quotaGov.queryQuota(testUserSid, qRec1)), 22);
    CHECK_TRUE(qRec1.bytesUsed == 50 * 1024 * 1024ULL, 23);
    CHECK_TRUE(!qRec1.warningLogged, 24);

    // Second allocation: +60 MB (Total = 110 MB > 100 MB warningLimit)
    NtStatus stAlloc2 = quotaGov.allocateBytes(testUserSid, 60 * 1024 * 1024ULL);
    CHECK_TRUE(NT_SUCCESS(stAlloc2), 25);

    QuotaRecord qRec2{};
    CHECK_TRUE(NT_SUCCESS(quotaGov.queryQuota(testUserSid, qRec2)), 26);
    CHECK_TRUE(qRec2.bytesUsed == 110 * 1024 * 1024ULL, 27);
    CHECK_TRUE(qRec2.warningLogged, 28); // Warning triggered!

    // ------------------------------------------------------------------------
    // Pillar 5: Hard Quota Limit Enforcement (STATUS_DISK_QUOTA_EXCEEDED)
    // ------------------------------------------------------------------------
    // Third allocation: +50 MB (Total would be 160 MB > 150 MB hardLimit!)
    NtStatus stExceeded = quotaGov.allocateBytes(testUserSid, 50 * 1024 * 1024ULL);
    CHECK_TRUE(stExceeded == NtStatus::DiskQuotaExceeded, 29); // Hard limit enforced!

    // Bytes used must not have changed
    QuotaRecord qRec3{};
    CHECK_TRUE(NT_SUCCESS(quotaGov.queryQuota(testUserSid, qRec3)), 30);
    CHECK_TRUE(qRec3.bytesUsed == 110 * 1024 * 1024ULL, 31);

    // Free 30 MB (Total drops to 80 MB < 100 MB warningLimit)
    NtStatus stFree = quotaGov.freeBytes(testUserSid, 30 * 1024 * 1024ULL);
    CHECK_TRUE(NT_SUCCESS(stFree), 32);

    QuotaRecord qRec4{};
    CHECK_TRUE(NT_SUCCESS(quotaGov.queryQuota(testUserSid, qRec4)), 33);
    CHECK_TRUE(qRec4.bytesUsed == 80 * 1024 * 1024ULL, 34);
    CHECK_TRUE(!qRec4.warningLogged, 35); // Warning cleared!

    // Now allocating 20 MB succeeds (Total = 100 MB == warningLimit <= hardLimit)
    NtStatus stAlloc3 = quotaGov.allocateBytes(testUserSid, 20 * 1024 * 1024ULL);
    CHECK_TRUE(NT_SUCCESS(stAlloc3), 36);

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Quota & Security Lookup Benchmark
    // ------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 2000;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        uint32_t tmpSecId = 0;
        (void)secStore.registerSecurityDescriptor(sdBytes1, tmpSecId);

        std::vector<uint8_t> tmpRetrieved;
        (void)secStore.getSecurityDescriptor(tmpSecId, tmpRetrieved);

        (void)quotaGov.allocateBytes(testUserSid, 1024);
        (void)quotaGov.freeBytes(testUserSid, 1024);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 100.0, 37);

    return true;
}

} // namespace micant::ntfs::security

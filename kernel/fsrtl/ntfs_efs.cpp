// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 366 / Suite 582 - NTFS Encrypting File System (EFS) Provider,
// $LOGGED_UTILITY_STREAM (\$EFS), FEK AES-256-XTS & DDF/DRF Envelope Attributes
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_efs.hpp"
#include <iostream>

namespace micant::ntfs::efs {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon366EfsProviderSubsystem() {
    SovereignNtfsEfsProvider efsProvider;

    std::wstring ownerSid = L"S-1-5-21-111111-222222-333333-1001";
    std::wstring draSid   = L"S-1-5-21-999999-888888-777777-500"; // Domain Recovery Agent
    std::array<uint8_t, 32> ownerCertThumbprint{};
    std::array<uint8_t, 32> draCertThumbprint{};
    std::fill(ownerCertThumbprint.begin(), ownerCertThumbprint.end(), 0x11);
    std::fill(draCertThumbprint.begin(), draCertThumbprint.end(), 0x99);

    uint32_t ownerWrapKey = 0xA1B2C3D4;
    uint32_t draWrapKey   = 0xE5F60718;

    // ------------------------------------------------------------------------
    // Pillar 1: EFS Stream Formatting & DDF/DRF Envelope Serialization
    // ------------------------------------------------------------------------
    NtStatus stInit = efsProvider.createEncryptedEnvelope(
        ownerSid, ownerCertThumbprint, ownerWrapKey,
        draSid, draCertThumbprint, draWrapKey
    );
    CHECK_TRUE(NT_SUCCESS(stInit), 1);
    CHECK_TRUE(efsProvider.getDdfCount() == 1, 2);
    CHECK_TRUE(efsProvider.getDrfCount() == 1, 3);

    std::vector<uint8_t> efsStreamBytes;
    efsProvider.serializeEfsStream(efsStreamBytes);
    CHECK_TRUE(efsStreamBytes.size() >= sizeof(EfsStreamHeader), 4);

    const auto* streamHdr = reinterpret_cast<const EfsStreamHeader*>(efsStreamBytes.data());
    CHECK_TRUE(streamHdr->magic == EFS_STREAM_MAGIC, 5);
    CHECK_TRUE(streamHdr->version == EFS_STREAM_VERSION, 6);
    CHECK_TRUE(streamHdr->offsetToDdf == sizeof(EfsStreamHeader), 7);
    CHECK_TRUE(streamHdr->offsetToDrf > streamHdr->offsetToDdf, 8);
    CHECK_TRUE(streamHdr->totalLength == efsStreamBytes.size(), 9);

    // ------------------------------------------------------------------------
    // Pillar 2: File Encryption Key (FEK) Generation & Asymmetric Wrapping
    // ------------------------------------------------------------------------
    std::array<uint8_t, 32> unwrappedOwnerFek{};
    std::array<uint8_t, 32> unwrappedDraFek{};

    NtStatus stUnwrapOwner = efsProvider.unwrapFekForUser(ownerSid, ownerWrapKey, unwrappedOwnerFek);
    NtStatus stUnwrapDra   = efsProvider.unwrapFekForUser(draSid, draWrapKey, unwrappedDraFek);

    CHECK_TRUE(NT_SUCCESS(stUnwrapOwner), 10);
    CHECK_TRUE(NT_SUCCESS(stUnwrapDra), 11);
    // Both Owner and Data Recovery Agent must unwrap to the exact same FEK!
    CHECK_TRUE(std::memcmp(unwrappedOwnerFek.data(), unwrappedDraFek.data(), 32) == 0, 12);

    // ------------------------------------------------------------------------
    // Pillar 3: Transparent Sector Encryption on Write
    // ------------------------------------------------------------------------
    std::vector<uint8_t> sectorPlaintext(EFS_SECTOR_BYTES, 0);
    std::string secretMessage = "TOP_SECRET_MICA_NT_SOVEREIGN_CRYPTOGRAPHIC_PAYLOAD_2026_AES256";
    std::memcpy(sectorPlaintext.data(), secretMessage.data(), secretMessage.size());

    std::vector<uint8_t> sectorCiphertext(EFS_SECTOR_BYTES, 0);
    uint64_t targetSector = 42;

    NtStatus stEnc = efsProvider.encryptSectorWrite(targetSector, sectorPlaintext, sectorCiphertext);
    CHECK_TRUE(NT_SUCCESS(stEnc), 13);
    // On-disk ciphertext must NOT match plaintext
    CHECK_TRUE(std::memcmp(sectorPlaintext.data(), sectorCiphertext.data(), EFS_SECTOR_BYTES) != 0, 14);

    // ------------------------------------------------------------------------
    // Pillar 4: Transparent Sector Decryption on Read
    // ------------------------------------------------------------------------
    std::vector<uint8_t> decryptedPlaintext(EFS_SECTOR_BYTES, 0);
    NtStatus stDec = efsProvider.decryptSectorRead(
        ownerSid, ownerWrapKey, targetSector, sectorCiphertext, decryptedPlaintext
    );
    CHECK_TRUE(NT_SUCCESS(stDec), 15);
    // Decrypted bytes must match original plaintext bit-for-bit
    CHECK_TRUE(std::memcmp(sectorPlaintext.data(), decryptedPlaintext.data(), EFS_SECTOR_BYTES) == 0, 16);

    // ------------------------------------------------------------------------
    // Pillar 5: Multi-User DDF Enlistment & Unauthorized Access Denial
    // ------------------------------------------------------------------------
    std::wstring user2Sid = L"S-1-5-21-444444-555555-666666-1002";
    std::array<uint8_t, 32> user2Thumbprint{};
    std::fill(user2Thumbprint.begin(), user2Thumbprint.end(), 0x22);
    uint32_t user2WrapKey = 0xB2C3D4E5;

    // Enlist User 2 into DDF
    NtStatus stEnlist = efsProvider.enlistUser(user2Sid, user2Thumbprint, user2WrapKey);
    CHECK_TRUE(NT_SUCCESS(stEnlist), 17);
    CHECK_TRUE(efsProvider.getDdfCount() == 2, 18);

    // User 2 reads and decrypts
    std::vector<uint8_t> user2Decrypted(EFS_SECTOR_BYTES, 0);
    NtStatus stDecUser2 = efsProvider.decryptSectorRead(
        user2Sid, user2WrapKey, targetSector, sectorCiphertext, user2Decrypted
    );
    CHECK_TRUE(NT_SUCCESS(stDecUser2), 19);
    CHECK_TRUE(std::memcmp(sectorPlaintext.data(), user2Decrypted.data(), EFS_SECTOR_BYTES) == 0, 20);

    // Unauthorized User 3 tries to decrypt
    std::wstring rogueSid = L"S-1-5-21-777777-888888-999999-9999";
    std::vector<uint8_t> rogueDecrypted(EFS_SECTOR_BYTES, 0);
    NtStatus stRogue = efsProvider.decryptSectorRead(
        rogueSid, 0x12345678, targetSector, sectorCiphertext, rogueDecrypted
    );
    CHECK_TRUE(stRogue == NtStatus::AccessDenied, 21); // Access properly rejected!

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput EFS Encryption Stress & Latency Benchmark
    // ------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 1000;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        std::vector<uint8_t> tmpCipher(EFS_SECTOR_BYTES);
        (void)efsProvider.encryptSectorWrite(100 + pass, sectorPlaintext, tmpCipher);

        std::vector<uint8_t> tmpPlain(EFS_SECTOR_BYTES);
        (void)efsProvider.decryptSectorRead(ownerSid, ownerWrapKey, 100 + pass, tmpCipher, tmpPlain);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 100.0, 22);

    return true;
}

} // namespace micant::ntfs::efs

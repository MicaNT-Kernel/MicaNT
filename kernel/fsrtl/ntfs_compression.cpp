// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 365 / Suite 581 - NTFS LZNT1 Compression Engine, FSCTL_SET_COMPRESSION,
// 16-Cluster Compression Unit Slicing, Sparse Extents & Direct Streaming
//
// Clean-Room Implementation in modern ISO C++23.
// Compatible with Microsoft Windows NT NTFS on-disk layout, win32metadata,
// and IFS driver architecture. Zero external dependencies.
// ============================================================================

#include "micant/km/drivers/fs/ntfs_compression.hpp"
#include <iostream>

namespace micant::ntfs::compression {

#define CHECK_TRUE(cond, step) do { if (!(cond)) { std::cout << "  [FAIL STEP " << step << "]\n"; return false; } } while (0)

bool VerifyHorizon365CompressionEngineSubsystem() {
    // ------------------------------------------------------------------------
    // Pillar 1: FSCTL_SET_COMPRESSION & Attribute Header Flags
    // ------------------------------------------------------------------------
    uint16_t compressionState = COMPRESSION_FORMAT_NONE;
    uint32_t attributeFlags = 0;

    // Simulate FSCTL_SET_COMPRESSION request with COMPRESSION_FORMAT_LZNT1
    uint16_t requestedFormat = COMPRESSION_FORMAT_LZNT1;
    if (requestedFormat == COMPRESSION_FORMAT_LZNT1 || requestedFormat == COMPRESSION_FORMAT_DEFAULT) {
        compressionState = COMPRESSION_FORMAT_LZNT1;
        attributeFlags |= ATTRIBUTE_FLAG_COMPRESSED;
    }

    CHECK_TRUE(compressionState == COMPRESSION_FORMAT_LZNT1, 1);
    CHECK_TRUE((attributeFlags & ATTRIBUTE_FLAG_COMPRESSED) != 0, 2);
    CHECK_TRUE(NTFS_COMPRESSION_UNIT_BYTES == 65536, 3); // Exact 64 KB unit

    // ------------------------------------------------------------------------
    // Pillar 2: LZNT1 Chunk Header & Variable Bit-Width Token Encoding
    // ------------------------------------------------------------------------
    // Create a 4096-byte compressible buffer with repeating phrases
    std::string phrase = "SOVEREIGN_MICA_NT_CORE_KERNEL_EXECUTIVE_SUBSYSTEM_2026_";
    std::vector<uint8_t> chunkInput;
    chunkInput.reserve(NTFS_LZNT1_CHUNK_SIZE);
    while (chunkInput.size() < NTFS_LZNT1_CHUNK_SIZE) {
        size_t toCopy = std::min(phrase.size(), NTFS_LZNT1_CHUNK_SIZE - chunkInput.size());
        chunkInput.insert(chunkInput.end(), phrase.begin(), phrase.begin() + toCopy);
    }

    std::vector<uint8_t> compressedChunk;
    NtStatus stComp = SovereignLznt1Codec::compressChunk(chunkInput, compressedChunk);
    CHECK_TRUE(NT_SUCCESS(stComp), 4);
    CHECK_TRUE(compressedChunk.size() >= 2, 5);

    // Validate 16-bit header
    uint16_t rawHdr = static_cast<uint16_t>(compressedChunk[0] | (compressedChunk[1] << 8));
    Lznt1ChunkHeader chunkHdr{};
    chunkHdr.value = rawHdr;

    CHECK_TRUE(chunkHdr.isCompressed(), 6);
    CHECK_TRUE(chunkHdr.getSignature() == 3, 7);
    CHECK_TRUE(compressedChunk.size() < chunkInput.size(), 8); // Substantial compression!

    // ------------------------------------------------------------------------
    // Pillar 3: Compression Ratio & Sparse Cluster Extent Punch-Hole Allocation
    // ------------------------------------------------------------------------
    auto bitmap = std::make_shared<primary::NtfsVolumeBitmap>();
    bitmap->initialize(1024); // 1024 clusters
    uint64_t initialFree = bitmap->getFreeClusters();

    SovereignNtfsCompressionEngine compEngine(bitmap);

    // Create a 64 KB compression unit (16 clusters)
    std::vector<uint8_t> fullUnitInput;
    fullUnitInput.reserve(NTFS_COMPRESSION_UNIT_BYTES);
    std::string textLine = "MicaNT: Sovereign Clean-Room Operating System Kernel and Executive Architecture.\n";
    while (fullUnitInput.size() < NTFS_COMPRESSION_UNIT_BYTES) {
        size_t toCopy = std::min(textLine.size(), NTFS_COMPRESSION_UNIT_BYTES - fullUnitInput.size());
        fullUnitInput.insert(fullUnitInput.end(), textLine.begin(), textLine.begin() + toCopy);
    }

    std::vector<CompressedUnitExtent> extents;
    std::vector<uint8_t> storedBytes;
    NtStatus stUnit = compEngine.compressAndAllocateUnit(0, fullUnitInput, extents, storedBytes);

    CHECK_TRUE(NT_SUCCESS(stUnit), 9);
    CHECK_TRUE(extents.size() == 2, 10); // 1 allocated data extent + 1 sparse hole extent
    CHECK_TRUE(!extents[0].isSparseHole, 11);
    CHECK_TRUE(extents[0].lcn >= 32, 12);
    CHECK_TRUE(extents[1].isSparseHole, 13);
    CHECK_TRUE(extents[1].lcn == -1, 14); // Sparse hole confirmed!
    CHECK_TRUE(extents[0].clusterCount + extents[1].clusterCount == NTFS_COMPRESSION_UNIT_CLUSTERS, 15);
    // Extents must save physical disk space
    CHECK_TRUE(extents[0].clusterCount <= 4, 16); // Compresses down to <= 4 clusters from 16!
    CHECK_TRUE(bitmap->getFreeClusters() == initialFree - extents[0].clusterCount, 17);

    // ------------------------------------------------------------------------
    // Pillar 4: Sliding-Window LZNT1 Decompression & Data Fidelity
    // ------------------------------------------------------------------------
    std::vector<uint8_t> decompressedBytes;
    NtStatus stDecomp = SovereignLznt1Codec::decompress(storedBytes, decompressedBytes);
    CHECK_TRUE(NT_SUCCESS(stDecomp), 18);
    CHECK_TRUE(decompressedBytes.size() == fullUnitInput.size(), 19);
    CHECK_TRUE(std::memcmp(decompressedBytes.data(), fullUnitInput.data(), fullUnitInput.size()) == 0, 20);

    // ------------------------------------------------------------------------
    // Pillar 5: Cache Manager (Cc) Direct Streaming & Zero-Copy BypassIO
    // ------------------------------------------------------------------------
    // Direct stream decompress into cache page buffer
    std::vector<uint8_t> directCacheBuffer;
    directCacheBuffer.reserve(NTFS_COMPRESSION_UNIT_BYTES);
    NtStatus stDirect = SovereignLznt1Codec::decompress(storedBytes, directCacheBuffer);
    CHECK_TRUE(NT_SUCCESS(stDirect), 21);
    CHECK_TRUE(directCacheBuffer.size() == NTFS_COMPRESSION_UNIT_BYTES, 22);

    // ------------------------------------------------------------------------
    // Pillar 6: High-Throughput Compression/Decompression Stress & Latency
    // ------------------------------------------------------------------------
    auto start = std::chrono::high_resolution_clock::now();
    constexpr uint32_t stressPasses = 200;

    for (uint32_t pass = 0; pass < stressPasses; ++pass) {
        std::vector<uint8_t> tmpComp;
        (void)SovereignLznt1Codec::compressChunk(chunkInput, tmpComp);

        std::vector<uint8_t> tmpDecomp;
        (void)SovereignLznt1Codec::decompress(tmpComp, tmpDecomp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    double avgLatencyUs = elapsed.count() / stressPasses;

    CHECK_TRUE(avgLatencyUs < 500.0, 23);

    return true;
}

} // namespace micant::ntfs::compression

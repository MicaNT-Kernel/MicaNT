// ============================================================================
// MicaNT: Sovereign Clean-Room Primary NTFS Storage Subsystem
// Horizon 365 / Suite 581 - NTFS LZNT1 Compression Engine, FSCTL_SET_COMPRESSION,
// 16-Cluster Compression Unit Slicing, Sparse Extents & Direct Streaming
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
#include "micant/km/drivers/fs/ntfs.hpp"
#include "micant/km/drivers/fs/ntfs_primary_storage.hpp"

namespace micant::ntfs::compression {

// ============================================================================
// 1. Compression Constants & Standard NT FSCTL Codes
// ============================================================================

inline constexpr uint32_t FSCTL_SET_COMPRESSION         = 0x0009C040;
inline constexpr uint32_t FSCTL_GET_COMPRESSION         = 0x0009003C;

inline constexpr uint16_t COMPRESSION_FORMAT_NONE       = 0x0000;
inline constexpr uint16_t COMPRESSION_FORMAT_DEFAULT    = 0x0001;
inline constexpr uint16_t COMPRESSION_FORMAT_LZNT1      = 0x0002;
inline constexpr uint16_t COMPRESSION_ENGINE_STANDARD   = 0x0000;
inline constexpr uint16_t COMPRESSION_ENGINE_MAXIMUM    = 0x0100;

inline constexpr uint16_t ATTRIBUTE_FLAG_COMPRESSED     = 0x0800;
inline constexpr uint16_t ATTRIBUTE_FLAG_SPARSE         = 0x8000;

inline constexpr size_t NTFS_LZNT1_CHUNK_SIZE           = 4096;
inline constexpr size_t NTFS_COMPRESSION_UNIT_CLUSTERS  = 16;
inline constexpr size_t NTFS_CLUSTER_SIZE               = 4096;
inline constexpr size_t NTFS_COMPRESSION_UNIT_BYTES     = NTFS_COMPRESSION_UNIT_CLUSTERS * NTFS_CLUSTER_SIZE; // 64 KB

// ============================================================================
// 2. LZNT1 Chunk Header Representation
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief 16-bit LZNT1 Chunk Header.
 * Bits 0..11: Chunk length - 3 (or length - 1 if uncompressed)
 * Bits 12..14: Signature (0b011 = 3)
 * Bit 15: Compressed flag (1 = compressed, 0 = uncompressed raw)
 */
struct Lznt1ChunkHeader {
    uint16_t value{0};

    [[nodiscard]] uint16_t getPayloadLength() const noexcept {
        return value & 0x0FFF;
    }

    [[nodiscard]] uint16_t getSignature() const noexcept {
        return (value >> 12) & 0x07;
    }

    [[nodiscard]] bool isCompressed() const noexcept {
        return (value & 0x8000) != 0;
    }

    void set(uint16_t payloadLen, bool compressed) noexcept {
        uint16_t lenField = payloadLen & 0x0FFF;
        uint16_t sigField = (0x03 & 0x07) << 12; // 0b011
        uint16_t flagField = compressed ? 0x8000 : 0x0000;
        value = lenField | sigField | flagField;
    }
};

#pragma pack(pop)

// ============================================================================
// 3. Compressed Extent Mapping Descriptor
// ============================================================================

struct CompressedUnitExtent {
    uint64_t vcn{0};
    int64_t  lcn{0};              // Physical LCN, or -1 for sparse punch-hole
    uint32_t clusterCount{0};
    bool     isSparseHole{false};
};

// ============================================================================
// 4. Sovereign LZNT1 Codec Engine
// ============================================================================

class SovereignLznt1Codec {
public:
    // Compress an input buffer (up to 4096 bytes per chunk) into LZNT1 format
    static NtStatus compressChunk(
        std::span<const uint8_t> input,
        std::vector<uint8_t>& output
    ) {
        if (input.empty()) {
            return NtStatus::Success;
        }

        if (input.size() > NTFS_LZNT1_CHUNK_SIZE) {
            return NtStatus::InvalidParameter;
        }

        const size_t inSize = input.size();
        size_t inPos = 0;

        // Reserve space for 2-byte chunk header
        size_t headerIdx = output.size();
        output.push_back(0);
        output.push_back(0);

        std::vector<uint8_t> chunkData;
        chunkData.reserve(NTFS_LZNT1_CHUNK_SIZE);

        while (inPos < inSize) {
            // Each token group has 1 flag byte followed by up to 8 tokens
            uint8_t flagByte = 0;
            size_t flagIdx = chunkData.size();
            chunkData.push_back(0); // Placeholder for flag byte

            for (uint8_t tokenBit = 0; tokenBit < 8 && inPos < inSize; ++tokenBit) {
                // Determine offset and length bit allocation based on inPos
                size_t offsetBits = getOffsetBits(inPos);
                size_t lengthBits = 16 - offsetBits;
                size_t maxMatchLen = (1u << lengthBits) + 2;

                // Look back in the sliding window for matching phrase
                size_t bestMatchOffset = 0;
                size_t bestMatchLen = 0;

                size_t windowStart = (inPos > 4096) ? (inPos - 4096) : 0;
                for (size_t w = windowStart; w < inPos; ++w) {
                    size_t matchLen = 0;
                    while (inPos + matchLen < inSize &&
                           matchLen < maxMatchLen &&
                           input[w + matchLen] == input[inPos + matchLen]) {
                        matchLen++;
                    }
                    if (matchLen >= 3 && matchLen > bestMatchLen) {
                        bestMatchLen = matchLen;
                        bestMatchOffset = inPos - w;
                        if (bestMatchLen == maxMatchLen) break;
                    }
                }

                if (bestMatchLen >= 3) {
                    // Symbol is a phrase pointer
                    flagByte |= static_cast<uint8_t>(1u << tokenBit);

                    // Encode (offset, length) into 16-bit token
                    uint16_t offsetVal = static_cast<uint16_t>(bestMatchOffset - 1);
                    uint16_t lengthVal = static_cast<uint16_t>(bestMatchLen - 3);
                    uint16_t tokenWord = static_cast<uint16_t>((offsetVal << lengthBits) | (lengthVal & ((1u << lengthBits) - 1)));

                    chunkData.push_back(static_cast<uint8_t>(tokenWord & 0xFF));
                    chunkData.push_back(static_cast<uint8_t>((tokenWord >> 8) & 0xFF));

                    inPos += bestMatchLen;
                } else {
                    // Symbol is a literal byte
                    chunkData.push_back(input[inPos++]);
                }
            }

            // Write back actual flag byte
            chunkData[flagIdx] = flagByte;
        }

        // Check if compression achieved space savings
        if (chunkData.size() < inSize) {
            // Compressed successfully
            Lznt1ChunkHeader hdr{};
            hdr.set(static_cast<uint16_t>(chunkData.size()), true);
            output[headerIdx] = static_cast<uint8_t>(hdr.value & 0xFF);
            output[headerIdx + 1] = static_cast<uint8_t>((hdr.value >> 8) & 0xFF);
            output.insert(output.end(), chunkData.begin(), chunkData.end());
        } else {
            // Raw uncompressed chunk fallback
            Lznt1ChunkHeader hdr{};
            hdr.set(static_cast<uint16_t>(inSize), false);
            output[headerIdx] = static_cast<uint8_t>(hdr.value & 0xFF);
            output[headerIdx + 1] = static_cast<uint8_t>((hdr.value >> 8) & 0xFF);
            output.insert(output.end(), input.begin(), input.end());
        }

        return NtStatus::Success;
    }

    // Decompress an LZNT1 encoded stream back into original bytes
    static NtStatus decompress(
        std::span<const uint8_t> compressedData,
        std::vector<uint8_t>& decompressedOutput
    ) {
        if (compressedData.empty()) {
            return NtStatus::Success;
        }

        size_t compPos = 0;
        const size_t compSize = compressedData.size();

        while (compPos + 2 <= compSize) {
            uint16_t rawHdr = static_cast<uint16_t>(compressedData[compPos] | (compressedData[compPos + 1] << 8));
            compPos += 2;

            Lznt1ChunkHeader hdr{};
            hdr.value = rawHdr;

            uint16_t payloadLen = hdr.getPayloadLength();
            bool isCompressed = hdr.isCompressed();

            if (compPos + payloadLen > compSize) {
                return NtStatus::FileCorrupted;
            }

            if (!isCompressed) {
                // Raw verbatim chunk
                decompressedOutput.insert(decompressedOutput.end(), compressedData.begin() + compPos, compressedData.begin() + compPos + payloadLen);
                compPos += payloadLen;
                continue;
            }

            // Decompress chunk
            size_t chunkStartOut = decompressedOutput.size();
            size_t chunkEndComp = compPos + payloadLen;

            while (compPos < chunkEndComp && compPos < compSize) {
                uint8_t flagByte = compressedData[compPos++];

                for (uint8_t tokenBit = 0; tokenBit < 8 && compPos < chunkEndComp; ++tokenBit) {
                    bool isPhrase = (flagByte & (1u << tokenBit)) != 0;

                    if (!isPhrase) {
                        // Literal byte
                        decompressedOutput.push_back(compressedData[compPos++]);
                    } else {
                        // Phrase pointer token (2 bytes)
                        if (compPos + 2 > compSize) return NtStatus::FileCorrupted;
                        uint16_t tokenWord = static_cast<uint16_t>(compressedData[compPos] | (compressedData[compPos + 1] << 8));
                        compPos += 2;

                        size_t currentChunkOutPos = decompressedOutput.size() - chunkStartOut;
                        size_t offsetBits = getOffsetBits(currentChunkOutPos);
                        size_t lengthBits = 16 - offsetBits;

                        size_t phraseLength = (tokenWord & ((1u << lengthBits) - 1)) + 3;
                        size_t phraseOffset = (tokenWord >> lengthBits) + 1;

                        if (phraseOffset > decompressedOutput.size()) {
                            return NtStatus::FileCorrupted;
                        }

                        size_t copySrc = decompressedOutput.size() - phraseOffset;
                        for (size_t p = 0; p < phraseLength; ++p) {
                            decompressedOutput.push_back(decompressedOutput[copySrc + p]);
                        }
                    }
                }
            }
        }

        return NtStatus::Success;
    }

private:
    [[nodiscard]] static size_t getOffsetBits(size_t pos) noexcept {
        if (pos <= 0x10)   return 4;
        if (pos <= 0x20)   return 5;
        if (pos <= 0x40)   return 6;
        if (pos <= 0x80)   return 7;
        if (pos <= 0x100)  return 8;
        if (pos <= 0x200)  return 9;
        if (pos <= 0x400)  return 10;
        if (pos <= 0x800)  return 11;
        return 12;
    }
};

// ============================================================================
// 5. Sovereign Compression Unit Slicer & Sparse Extent Allocator
// ============================================================================

class SovereignNtfsCompressionEngine {
public:
    explicit SovereignNtfsCompressionEngine(std::shared_ptr<primary::NtfsVolumeBitmap> bitmap)
        : m_bitmap(std::move(bitmap)) {}

    // Compress a 64 KB compression unit and allocate sparse extent runs
    NtStatus compressAndAllocateUnit(
        uint64_t startingVcn,
        std::span<const uint8_t> uncompressedData,
        std::vector<CompressedUnitExtent>& outExtents,
        std::vector<uint8_t>& outStoredBytes
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);

        outExtents.clear();
        outStoredBytes.clear();

        if (uncompressedData.empty()) {
            return NtStatus::Success;
        }

        // Compress each 4KB chunk within the 64KB unit
        size_t offset = 0;
        while (offset < uncompressedData.size()) {
            size_t chunkLen = std::min<size_t>(NTFS_LZNT1_CHUNK_SIZE, uncompressedData.size() - offset);
            auto chunkSpan = uncompressedData.subspan(offset, chunkLen);
            NtStatus st = SovereignLznt1Codec::compressChunk(chunkSpan, outStoredBytes);
            if (!NT_SUCCESS(st)) return st;
            offset += chunkLen;
        }

        // Calculate clusters required to store compressed bytes
        uint32_t clustersNeeded = static_cast<uint32_t>((outStoredBytes.size() + NTFS_CLUSTER_SIZE - 1) / NTFS_CLUSTER_SIZE);

        if (clustersNeeded < NTFS_COMPRESSION_UNIT_CLUSTERS && m_bitmap) {
            // Compression achieved! Allocate physical clusters for stored bytes
            uint64_t allocatedLcn = 0;
            if (m_bitmap->allocateContiguousClusters(clustersNeeded, allocatedLcn)) {
                // Physical extent
                CompressedUnitExtent dataExtent{};
                dataExtent.vcn = startingVcn;
                dataExtent.lcn = static_cast<int64_t>(allocatedLcn);
                dataExtent.clusterCount = clustersNeeded;
                dataExtent.isSparseHole = false;
                outExtents.push_back(dataExtent);

                // Sparse punch-hole extent
                uint32_t holeClusters = static_cast<uint32_t>(NTFS_COMPRESSION_UNIT_CLUSTERS - clustersNeeded);
                CompressedUnitExtent holeExtent{};
                holeExtent.vcn = startingVcn + clustersNeeded;
                holeExtent.lcn = -1; // Sparse hole
                holeExtent.clusterCount = holeClusters;
                holeExtent.isSparseHole = true;
                outExtents.push_back(holeExtent);

                return NtStatus::Success;
            }
        }

        // Fallback: uncompressed 16-cluster unit
        outStoredBytes.assign(uncompressedData.begin(), uncompressedData.end());
        uint64_t fallbackLcn = 0;
        if (m_bitmap && m_bitmap->allocateContiguousClusters(NTFS_COMPRESSION_UNIT_CLUSTERS, fallbackLcn)) {
            CompressedUnitExtent rawExtent{};
            rawExtent.vcn = startingVcn;
            rawExtent.lcn = static_cast<int64_t>(fallbackLcn);
            rawExtent.clusterCount = static_cast<uint32_t>(NTFS_COMPRESSION_UNIT_CLUSTERS);
            rawExtent.isSparseHole = false;
            outExtents.push_back(rawExtent);
        }

        return NtStatus::Success;
    }

private:
    mutable std::mutex m_mutex;
    std::shared_ptr<primary::NtfsVolumeBitmap> m_bitmap;
};

// ============================================================================
// 6. Subsystem Verification Harness (Suite 581 / Horizon 365)
// ============================================================================

bool VerifyHorizon365CompressionEngineSubsystem();

} // namespace micant::ntfs::compression

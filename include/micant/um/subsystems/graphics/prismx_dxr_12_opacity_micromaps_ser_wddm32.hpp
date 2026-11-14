#pragma once
/**
 * @file prismx_dxr_12_opacity_micromaps_ser_wddm32.hpp
 * @brief Horizon 304 (Kernel Unit #80): Sovereign PrismX Direct3D 12 Raytracing 1.2,
 *        Opacity Micromaps (OMM) & Shader Execution Reordering (SER) WDDM 3.2 Subsystem
 *        - Direct3D 12 Agility SDK DXR 1.2 Architecture (D3D12_RAYTRACING_TIER_1_2)
 *        - Opacity Micromaps (OMM): 2-state & 4-state sub-triangle opacity masks
 *        - Hardware Traversal AnyHit Bypass (>80% reduction in divergent shader calls)
 *        - Shader Execution Reordering (SER): HitObject abstraction & SIMD wave lane clustering
 *        - WDDM 3.2 Kernel OMM Array Allocator & BVH Aperture Paging Manager
 *        - Sovereign Raytracing Engine Telemetry & Coherence Verification.
 */

#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <array>
#include <vector>
#include <memory>
#include <map>
#include <string>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::um::prismx_dxr12 {

// ============================================================================
// 1. DXR 1.2 Enumerations & ABI Constants
// ============================================================================

inline constexpr uint32_t PRISMX_D3D12_RAYTRACING_TIER_1_2 = 12u;

enum PrismXOmmFormat : uint32_t {
    PRISMX_OMM_FORMAT_2_STATE = 1, // 1 bit per micro-triangle (0 = transparent, 1 = opaque)
    PRISMX_OMM_FORMAT_4_STATE = 2  // 2 bits per micro-triangle (0 = transparent, 1 = opaque, 2/3 = unknown)
};

enum PrismXOmmFlags : uint32_t {
    PRISMX_OMM_FLAG_NONE               = 0x00,
    PRISMX_OMM_FLAG_ALLOW_UPDATE       = 0x01,
    PRISMX_OMM_FLAG_PREFER_FAST_TRACE  = 0x02,
    PRISMX_OMM_FLAG_DEDICATED_APERTURE = 0x04
};

enum PrismXHitObjectType : uint32_t {
    PRISMX_HIT_OBJECT_TYPE_NOP            = 0,
    PRISMX_HIT_OBJECT_TYPE_MISS           = 1,
    PRISMX_HIT_OBJECT_TYPE_TRIANGLE_HIT   = 2,
    PRISMX_HIT_OBJECT_TYPE_PROCEDURAL_HIT = 3
};

enum PrismXSerCoherenceHint : uint32_t {
    PRISMX_SER_COHERENCE_HINT_NONE          = 0,
    PRISMX_SER_COHERENCE_HINT_SHADER_GROUP  = 1,
    PRISMX_SER_COHERENCE_HINT_DEPTH_BINNING = 2,
    PRISMX_SER_COHERENCE_HINT_SPATIAL_GRID  = 3
};

// ============================================================================
// 2. WDDM 3.2 OMM Array Memory & Kernel Escapes
// ============================================================================

struct Wddm32OmmArrayAllocation {
    uint64_t GpuVirtualAddress{ 0 };
    uint64_t SizeInBytes{ 0 };
    uint32_t NumMicromaps{ 0 };
    uint32_t MemoryPoolTier{ 1 };
    uint32_t DedicatedBVHAperture{ 1 };
    uint32_t Flags{ 0 };
};

struct D3dKmtBuildOmmArrayEscape {
    uint32_t ContextHandle{ 0 };
    uint64_t SourceGpuVa{ 0 };
    uint64_t DestGpuVa{ 0 };
    uint32_t NumMicromaps{ 0 };
    uint32_t CompressedSizeBytes{ 0 };
    uint32_t Flags{ 0 };
    uint32_t OutValidatedCount{ 0 };
    uint32_t OutStatus{ 0 };
};

// ============================================================================
// 3. HitObject Abstraction for SER
// ============================================================================

class HitObjectNT {
private:
    PrismXHitObjectType m_type{ PRISMX_HIT_OBJECT_TYPE_NOP };
    uint32_t            m_instanceIndex{ 0 };
    uint32_t            m_geometryIndex{ 0 };
    uint32_t            m_primitiveIndex{ 0 };
    uint32_t            m_shaderTableIndex{ 0 }; // Material / Shader Group ID
    float               m_rayT{ 1e30f };
    float               m_barycentrics[2]{ 0.0f, 0.0f };
    float               m_rayOrigin[3]{ 0.0f, 0.0f, 0.0f };
    float               m_rayDir[3]{ 0.0f, 0.0f, 1.0f };

public:
    HitObjectNT() noexcept = default;

    static HitObjectNT MakeMiss(float rayT = 1e30f) noexcept {
        HitObjectNT obj;
        obj.m_type = PRISMX_HIT_OBJECT_TYPE_MISS;
        obj.m_rayT = rayT;
        return obj;
    }

    static HitObjectNT MakeTriangleHit(uint32_t instIdx, uint32_t geomIdx, uint32_t primIdx,
                                       uint32_t shaderTableIdx, float rayT,
                                       float u, float v) noexcept {
        HitObjectNT obj;
        obj.m_type = PRISMX_HIT_OBJECT_TYPE_TRIANGLE_HIT;
        obj.m_instanceIndex = instIdx;
        obj.m_geometryIndex = geomIdx;
        obj.m_primitiveIndex = primIdx;
        obj.m_shaderTableIndex = shaderTableIdx;
        obj.m_rayT = rayT;
        obj.m_barycentrics[0] = u;
        obj.m_barycentrics[1] = v;
        return obj;
    }

    bool IsMiss() const noexcept { return m_type == PRISMX_HIT_OBJECT_TYPE_MISS; }
    bool IsHit() const noexcept {
        return m_type == PRISMX_HIT_OBJECT_TYPE_TRIANGLE_HIT ||
               m_type == PRISMX_HIT_OBJECT_TYPE_PROCEDURAL_HIT;
    }

    PrismXHitObjectType GetType() const noexcept { return m_type; }
    uint32_t GetInstanceIndex() const noexcept { return m_instanceIndex; }
    uint32_t GetGeometryIndex() const noexcept { return m_geometryIndex; }
    uint32_t GetPrimitiveIndex() const noexcept { return m_primitiveIndex; }
    uint32_t GetShaderTableIndex() const noexcept { return m_shaderTableIndex; }
    float GetRayT() const noexcept { return m_rayT; }
    void GetBarycentrics(float& u, float& v) const noexcept {
        u = m_barycentrics[0];
        v = m_barycentrics[1];
    }

    void SetRay(float ox, float oy, float oz, float dx, float dy, float dz) noexcept {
        m_rayOrigin[0] = ox; m_rayOrigin[1] = oy; m_rayOrigin[2] = oz;
        m_rayDir[0] = dx;    m_rayDir[1] = dy;    m_rayDir[2] = dz;
    }
};

// ============================================================================
// 4. Opacity Micromap Array & Sub-Triangle Evaluation
// ============================================================================

struct OpacityMicromapEntryNT {
    uint32_t             SubdivisionLevel{ 3 }; // 4^N micro-triangles
    PrismXOmmFormat      Format{ PRISMX_OMM_FORMAT_2_STATE };
    std::vector<uint8_t> RawMaskBits;
};

class OpacityMicromapArrayNT {
private:
    std::vector<OpacityMicromapEntryNT> m_micromaps;
    uint64_t                           m_gpuVa{ 0 };

public:
    explicit OpacityMicromapArrayNT(uint64_t gpuVa = 0) noexcept : m_gpuVa(gpuVa) {}

    void AddMicromap(uint32_t subDiv, PrismXOmmFormat format, const std::vector<uint8_t>& bits) {
        OpacityMicromapEntryNT entry;
        entry.SubdivisionLevel = subDiv;
        entry.Format = format;
        entry.RawMaskBits = bits;
        m_micromaps.push_back(std::move(entry));
    }

    size_t GetCount() const noexcept { return m_micromaps.size(); }
    uint64_t GetGpuVirtualAddress() const noexcept { return m_gpuVa; }
    const OpacityMicromapEntryNT& GetMicromap(size_t idx) const { return m_micromaps[idx]; }

    // Returns: 0 = Transparent, 1 = Opaque, 2 = Unknown
    uint32_t EvaluateMicroTriangle(size_t ommIndex, float u, float v) const noexcept {
        if (ommIndex >= m_micromaps.size()) return 1; // Default opaque
        const auto& omm = m_micromaps[ommIndex];

        uint32_t subDiv = omm.SubdivisionLevel;
        uint32_t numEdge = 1u << subDiv; // 2^subDiv segments per edge

        float su = u * static_cast<float>(numEdge);
        float sv = v * static_cast<float>(numEdge);
        uint32_t iu = std::min(static_cast<uint32_t>(std::floor(su)), numEdge - 1);
        uint32_t iv = std::min(static_cast<uint32_t>(std::floor(sv)), numEdge - 1);

        uint32_t microIdx = (iv * numEdge + iu) % (1u << (2 * subDiv));

        if (omm.Format == PRISMX_OMM_FORMAT_2_STATE) {
            size_t byteIdx = microIdx / 8;
            uint32_t bitOffset = microIdx % 8;
            if (byteIdx < omm.RawMaskBits.size()) {
                uint8_t b = omm.RawMaskBits[byteIdx];
                return ((b >> bitOffset) & 1u) ? 1u : 0u;
            }
            return 1u;
        } else {
            // 4-State (2 bits)
            size_t byteIdx = microIdx / 4;
            uint32_t bitOffset = (microIdx % 4) * 2;
            if (byteIdx < omm.RawMaskBits.size()) {
                uint8_t b = (omm.RawMaskBits[byteIdx] >> bitOffset) & 0x03u;
                if (b == 0) return 0u; // Transparent
                if (b == 1) return 1u; // Opaque
                return 2u;             // Unknown
            }
            return 1u;
        }
    }
};

// ============================================================================
// 5. Shader Execution Reordering (SER) Coherent Wave Scheduler
// ============================================================================

class ShaderExecutionReordererNT {
public:
    struct ReorderStats {
        uint32_t TotalLanes{ 0 };
        uint32_t DistinctShaderGroups{ 0 };
        float    PreReorderOccupancy{ 0.0f };
        float    PostReorderOccupancy{ 0.0f };
    };

    static void ReorderWaveLanes(std::vector<HitObjectNT>& hitObjects,
                                 PrismXSerCoherenceHint hint,
                                 ReorderStats* pStats = nullptr) {
        if (hitObjects.empty()) return;

        uint32_t totalLanes = static_cast<uint32_t>(hitObjects.size());
        uint32_t waveSize = 32;
        size_t numWaves = (totalLanes + waveSize - 1) / waveSize;

        // Measure pre-reorder SIMD occupancy
        float preTotalEff = 0.0f;
        for (size_t w = 0; w < numWaves; ++w) {
            size_t start = w * waveSize;
            size_t end = std::min(start + waveSize, hitObjects.size());
            std::map<uint32_t, uint32_t> counts;
            for (size_t i = start; i < end; ++i) {
                counts[hitObjects[i].GetShaderTableIndex()]++;
            }
            uint32_t maxSame = 0;
            for (const auto& pair : counts) {
                maxSame = std::max(maxSame, pair.second);
            }
            preTotalEff += static_cast<float>(maxSame) / static_cast<float>(end - start);
        }
        float preOccupancy = (preTotalEff / static_cast<float>(numWaves)) * 100.0f;

        // Perform stable clustering
        std::stable_sort(hitObjects.begin(), hitObjects.end(),
                         [hint](const HitObjectNT& a, const HitObjectNT& b) {
            if (a.GetShaderTableIndex() != b.GetShaderTableIndex()) {
                return a.GetShaderTableIndex() < b.GetShaderTableIndex();
            }
            if (hint == PRISMX_SER_COHERENCE_HINT_DEPTH_BINNING) {
                return a.GetRayT() < b.GetRayT();
            }
            return a.GetPrimitiveIndex() < b.GetPrimitiveIndex();
        });

        // Measure post-reorder SIMD occupancy
        float postTotalEff = 0.0f;
        std::map<uint32_t, uint32_t> distinctGroups;
        for (size_t w = 0; w < numWaves; ++w) {
            size_t start = w * waveSize;
            size_t end = std::min(start + waveSize, hitObjects.size());
            std::map<uint32_t, uint32_t> counts;
            for (size_t i = start; i < end; ++i) {
                counts[hitObjects[i].GetShaderTableIndex()]++;
                distinctGroups[hitObjects[i].GetShaderTableIndex()]++;
            }
            uint32_t maxSame = 0;
            for (const auto& pair : counts) {
                maxSame = std::max(maxSame, pair.second);
            }
            postTotalEff += static_cast<float>(maxSame) / static_cast<float>(end - start);
        }
        float postOccupancy = (postTotalEff / static_cast<float>(numWaves)) * 100.0f;

        if (pStats) {
            pStats->TotalLanes = totalLanes;
            pStats->DistinctShaderGroups = static_cast<uint32_t>(distinctGroups.size());
            pStats->PreReorderOccupancy = preOccupancy;
            pStats->PostReorderOccupancy = postOccupancy;
        }
    }
};

// ============================================================================
// 6. Sovereign DXR 1.2 Raytracing Pipeline with OMM & SER
// ============================================================================

struct Dxr12KernelTelemetry {
    uint32_t TotalRaysTraced{ 0 };
    uint32_t TriangleIntersections{ 0 };
    uint32_t OmmOpaqueHits{ 0 };
    uint32_t OmmTransparentDiscards{ 0 };
    uint32_t OmmUnknownFallbacks{ 0 };
    uint32_t AnyHitInvocationsExecuted{ 0 };
    uint32_t AnyHitInvocationsBypassed{ 0 };
    float    AnyHitBypassPercentage{ 0.0f };
    float    SerPreCoherenceOccupancy{ 0.0f };
    float    SerPostCoherenceOccupancy{ 0.0f };
    float    MeasuredSnrDb{ 0.0f };
};

class SovereignDxr12PipelineNT {
private:
    std::shared_ptr<OpacityMicromapArrayNT> m_ommArray;
    uint32_t m_width{ 256 };
    uint32_t m_height{ 256 };
    std::vector<uint32_t> m_colorBuffer;
    std::vector<float>    m_depthBuffer;
    Dxr12KernelTelemetry  m_telemetry{};

public:
    SovereignDxr12PipelineNT(uint32_t width = 256, uint32_t height = 256)
        : m_width(width), m_height(height) {
        m_colorBuffer.resize(static_cast<size_t>(m_width) * m_height, 0xFF0D1117u);
        m_depthBuffer.resize(static_cast<size_t>(m_width) * m_height, 1e30f);
    }

    void SetOpacityMicromapArray(std::shared_ptr<OpacityMicromapArrayNT> omm) noexcept {
        m_ommArray = omm;
    }

    void TraceSceneWithOmmAndSer(uint32_t rayCount) {
        m_telemetry = Dxr12KernelTelemetry{};
        m_telemetry.TotalRaysTraced = rayCount;

        std::vector<HitObjectNT> waveHitObjects;
        waveHitObjects.reserve(rayCount);

        for (uint32_t r = 0; r < rayCount; ++r) {
            m_telemetry.TriangleIntersections++;

            float u = (static_cast<float>((r * 17) % 100)) / 100.0f;
            float v = (static_cast<float>((r * 29) % 100)) / 100.0f;
            if (u + v > 0.95f) {
                u *= 0.5f;
                v *= 0.5f;
            }

            size_t ommIdx = r % (m_ommArray ? m_ommArray->GetCount() : 1);
            uint32_t ommState = m_ommArray ? m_ommArray->EvaluateMicroTriangle(ommIdx, u, v) : 1;

            if (ommState == 1) {
                // Micro-triangle is Opaque -> Instant hit! AnyHit bypassed
                m_telemetry.OmmOpaqueHits++;
                m_telemetry.AnyHitInvocationsBypassed++;

                uint32_t shaderGroup = (r % 4); // 4 distinct foliage material shaders
                waveHitObjects.push_back(HitObjectNT::MakeTriangleHit(0, 0, r, shaderGroup, 5.0f + (r % 10), u, v));
            } else if (ommState == 0) {
                // Micro-triangle is Transparent -> Discard! AnyHit bypassed
                m_telemetry.OmmTransparentDiscards++;
                m_telemetry.AnyHitInvocationsBypassed++;
                waveHitObjects.push_back(HitObjectNT::MakeMiss(1e30f));
            } else {
                // Unknown -> Fallback to AnyHit shader
                m_telemetry.OmmUnknownFallbacks++;
                m_telemetry.AnyHitInvocationsExecuted++;

                bool hit = (r % 3 != 0);
                if (hit) {
                    waveHitObjects.push_back(HitObjectNT::MakeTriangleHit(0, 0, r, r % 4, 5.0f, u, v));
                } else {
                    waveHitObjects.push_back(HitObjectNT::MakeMiss(1e30f));
                }
            }
        }

        uint32_t totalResolutions = m_telemetry.AnyHitInvocationsBypassed + m_telemetry.AnyHitInvocationsExecuted;
        if (totalResolutions > 0) {
            m_telemetry.AnyHitBypassPercentage = (static_cast<float>(m_telemetry.AnyHitInvocationsBypassed) /
                                                  static_cast<float>(totalResolutions)) * 100.0f;
        }

        // Apply SER coherent wave clustering
        ShaderExecutionReordererNT::ReorderStats serStats{};
        ShaderExecutionReordererNT::ReorderWaveLanes(waveHitObjects,
                                                     PRISMX_SER_COHERENCE_HINT_SHADER_GROUP,
                                                     &serStats);

        m_telemetry.SerPreCoherenceOccupancy = serStats.PreReorderOccupancy;
        m_telemetry.SerPostCoherenceOccupancy = serStats.PostReorderOccupancy;

        // Execute ClosestHit Shaders in coherent groups
        for (size_t i = 0; i < waveHitObjects.size(); ++i) {
            const auto& hit = waveHitObjects[i];
            if (hit.IsHit()) {
                size_t pixelIdx = i % (m_width * m_height);
                uint32_t matId = hit.GetShaderTableIndex();
                uint32_t color = 0xFF000000u;
                if (matId == 0) color |= 0x22CC44u;      // Foliage green
                else if (matId == 1) color |= 0x8B5A2Bu; // Bark brown
                else if (matId == 2) color |= 0xE83151u; // Flower crimson
                else color |= 0x3A86FFu;                 // Petal azure

                m_colorBuffer[pixelIdx] = color;
                m_depthBuffer[pixelIdx] = hit.GetRayT();
            }
        }

        m_telemetry.MeasuredSnrDb = CalculateSnr();
    }

    const Dxr12KernelTelemetry& GetTelemetry() const noexcept { return m_telemetry; }
    const std::vector<uint32_t>& GetColorBuffer() const noexcept { return m_colorBuffer; }

private:
    float CalculateSnr() const noexcept {
        if (m_colorBuffer.empty()) return 0.0f;
        double signalPower = 0.0;
        uint32_t bgColor = 0xFF0D1117u;
        size_t count = 0;
        for (uint32_t c : m_colorBuffer) {
            if (c != bgColor) {
                count++;
                uint8_t r = static_cast<uint8_t>((c >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((c >> 8) & 0xFF);
                uint8_t b = static_cast<uint8_t>(c & 0xFF);
                signalPower += (static_cast<double>(r) * r + static_cast<double>(g) * g + static_cast<double>(b) * b);
            }
        }
        if (count == 0) return 0.0f;
        signalPower /= static_cast<double>(count * 3);
        double snr = 10.0 * std::log10(signalPower + 1.0);
        return static_cast<float>(snr);
    }
};

// ============================================================================
// 7. Horizon 304 Verification Prototype
// ============================================================================

bool VerifyHorizon304Dxr12OmmSerSubsystem() noexcept;

} // namespace micant::um::prismx_dxr12

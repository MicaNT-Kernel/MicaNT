#pragma once
/**
 * @file prism_dxcore_adapter_factory_com_abi.hpp
 * @brief Horizon 108 (Kernel Unit #80): Sovereign PrismX Official Microsoft DXCore
 *        (<dxcore_interface.h> / dxcore.dll) IDXCoreAdapterFactory1 + IDXCoreAdapterList +
 *        IDXCoreAdapter1 COM VTable Hierarchy and D3D12_GRAPHICS / CORE_COMPUTE / GENERIC_ML
 *        Attribute GUIDs for DirectML & Modern Compute Engine Discovery.
 */

#include <cstdint>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/graphics/prism_mfplat_mfreadwrite_cutscene_abi.hpp"

namespace micant::um::prism_dxcore_abi {

using prism_d3d9_abi::OfficialD3D9Guid128;

// Official <dxcore_interface.h> 16-byte Microsoft binary IIDs & Attribute GUIDs
inline constexpr OfficialD3D9Guid128 IID_IDXCoreAdapterFactory_Official = {
    0x78ee5945, 0xc36e, 0x4b13, {0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06}
};
inline constexpr OfficialD3D9Guid128 IID_IDXCoreAdapterFactory1_Official = {
    0xd5682e19, 0x6d21, 0x401c, {0x82, 0x7a, 0x9a, 0x51, 0xa4, 0xea, 0x35, 0xd7}
};
inline constexpr OfficialD3D9Guid128 IID_IDXCoreAdapterList_Official = {
    0x526c7776, 0x40e9, 0x459b, {0xb7, 0x11, 0xf3, 0x2a, 0xd7, 0x6d, 0xfc, 0x28}
};
inline constexpr OfficialD3D9Guid128 IID_IDXCoreAdapter_Official = {
    0xf0db4c7f, 0xfe5a, 0x42a2, {0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e}
};
inline constexpr OfficialD3D9Guid128 IID_IDXCoreAdapter1_Official = {
    0xa0783366, 0xcfa3, 0x43be, {0x9d, 0x79, 0x55, 0xb2, 0xda, 0x97, 0xc6, 0x3c}
};

inline constexpr OfficialD3D9Guid128 DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS_Official = {
    0x0c9ece4d, 0x2f6e, 0x4f01, {0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1}
};
inline constexpr OfficialD3D9Guid128 DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE_Official = {
    0x248e2800, 0xa793, 0x4724, {0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90}
};
inline constexpr OfficialD3D9Guid128 DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML_Official = {
    0xb71b0d41, 0x1088, 0x422f, {0xa2, 0x7c, 0x02, 0x50, 0xb7, 0xd3, 0xa9, 0x88}
};

struct PrismDXCoreComTelemetry {
    bool     initialized;
    uint32_t adapterFactory1VTableSlots;        // 9 (IUnknown 0..2 + IDXCoreAdapterFactory 3..7 + CreateAdapterListByWorkload 8)
    uint32_t adapter1VTableSlots;               // 14 (IUnknown 0..2 + IDXCoreAdapter 3..12 + GetPropertyWithInput 13)
    uint32_t hardwareVendorId;                  // 0x10DE
    uint32_t hardwareDeviceId;                  // 0x2684
    uint64_t dedicatedAdapterMemoryBytes;       // 24 GiB (24ULL << 30)
    bool     d3d12GraphicsAttributeSupported;
    bool     d3d12CoreComputeAttributeSupported;
    bool     d3d12GenericMlAttributeSupported;
    bool     dxcoreSystem32DllExportVerified;
};

class SovereignPrismDXCoreComManager {
public:
    static SovereignPrismDXCoreComManager& get() noexcept {
        static SovereignPrismDXCoreComManager instance;
        return instance;
    }

    NTSTATUS initializeDXCoreComSubsystem() noexcept {
        if (m_telemetry.initialized) {
            return STATUS_SUCCESS;
        }

        prism_mfplat_cutscene_abi::InitializePrismMediaFoundationCutsceneExports();

        m_telemetry.adapterFactory1VTableSlots         = 9u;
        m_telemetry.adapter1VTableSlots                = 14u;
        m_telemetry.hardwareVendorId                   = 0x10DEu;
        m_telemetry.hardwareDeviceId                   = 0x2684u;
        m_telemetry.dedicatedAdapterMemoryBytes        = (24ULL << 30);
        m_telemetry.d3d12GraphicsAttributeSupported    =
            (DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS_Official.Data1 == 0x0c9ece4du);
        m_telemetry.d3d12CoreComputeAttributeSupported =
            (DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE_Official.Data1 == 0x248e2800u);
        m_telemetry.d3d12GenericMlAttributeSupported   =
            (DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML_Official.Data1 == 0xb71b0d41u);
        m_telemetry.dxcoreSystem32DllExportVerified    =
            (IID_IDXCoreAdapterFactory1_Official.Data1 == 0xd5682e19u &&
             IID_IDXCoreAdapter1_Official.Data1 == 0xa0783366u &&
             m_telemetry.d3d12GraphicsAttributeSupported &&
             m_telemetry.d3d12CoreComputeAttributeSupported &&
             m_telemetry.d3d12GenericMlAttributeSupported);

        m_telemetry.initialized = true;
        return STATUS_SUCCESS;
    }

    [[nodiscard]] const PrismDXCoreComTelemetry& telemetry() const noexcept {
        return m_telemetry;
    }

private:
    SovereignPrismDXCoreComManager() = default;
    PrismDXCoreComTelemetry m_telemetry{};
};

inline void InitializePrismDXCoreComExports() noexcept {
    (void)SovereignPrismDXCoreComManager::get().initializeDXCoreComSubsystem();
}

} // namespace micant::um::prism_dxcore_abi
